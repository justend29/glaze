// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <charconv>
#include <iterator>
#include <ostream>
#include <variant>

#include "glaze/core/format.hpp"
#include "glaze/core/write.hpp"
#include "glaze/core/write_chars.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/util/itoa.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_json
      {};

      template <>
      struct write<json>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            to_json<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                        std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <glaze_value_t T>
      struct to_json<T>
      {
         template <auto Opts, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, B&& b, IX&& ix)
         {
            using V = std::decay_t<decltype(get_member(std::declval<T>(), meta_wrapper_v<T>))>;
            to_json<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx),
                                          std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <glaze_flags_t T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix)
         {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;

            dump<'['>(b, ix);

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<T>);

               if (get_member(value, glz::tuplet::get<1>(item))) {
                  dump<'"'>(b, ix);
                  dump(glz::tuplet::get<0>(item), b, ix);
                  dump<'"'>(b, ix);
                  dump<','>(b, ix);
               }
            });

            if (b[ix - 1] == ',') {
               b[ix - 1] = ']';
            }
            else {
               dump<']'>(b, ix);
            }
         }
      };

      template <>
      struct to_json<hidden>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&... args)
         {
            dump(R"("hidden type should not have been written")", args...);
         }
      };

      template <>
      struct to_json<skip>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&... args)
         {
            dump(R"("skip type should not have been written")", args...);
         }
      };

      template <is_member_function_pointer T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&...)
         {}
      };

      template <is_reference_wrapper T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args)
         {
            using V = std::decay_t<decltype(value.get())>;
            to_json<V>::template op<Opts>(value.get(), std::forward<Args>(args)...);
         }
      };

      template <boolean_like T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, Args&&... args) noexcept
         {
            if (value) {
               dump<"true">(std::forward<Args>(args)...);
            }
            else {
               dump<"false">(std::forward<Args>(args)...);
            }
         }
      };

      template <num_t T>
      struct to_json<T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, B&& b, auto&& ix) noexcept
         {
            if constexpr (Opts.quoted) {
               dump<'"'>(b, ix);
            }
            write_chars::op<Opts>(value, ctx, b, ix);
            if constexpr (Opts.quoted) {
               dump<'"'>(b, ix);
            }
         }
      };

      template <class T>
         requires str_t<T> || char_t<T>
      struct to_json<T>
      {
         template <auto Opts, class B>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, B&& b, auto&& ix) noexcept
         {
            if constexpr (Opts.number) {
               // TODO: Should we check if the string number is valid?
               dump(value, b, ix);
            }
            else {
               if constexpr (char_t<T>) {
                  dump<'"'>(b, ix);
                  switch (value) {
                  case '"':
                     dump<"\\\"">(b, ix);
                     break;
                  case '\\':
                     dump<"\\\\">(b, ix);
                     break;
                  case '\b':
                     dump<"\\b">(b, ix);
                     break;
                  case '\f':
                     dump<"\\f">(b, ix);
                     break;
                  case '\n':
                     dump<"\\n">(b, ix);
                     break;
                  case '\r':
                     dump<"\\r">(b, ix);
                     break;
                  case '\t':
                     dump<"\\t">(b, ix);
                     break;
                  default:
                     dump(value, b, ix); // TODO: This warning is an error We need to be able to dump wider char types
                  }
                  dump<'"'>(b, ix);
               }
               else {
                  const sv str = value;
                  const auto n = str.size();

                  // we use 4 * n to handle potential escape characters and quoted bounds
                  // Example: if n were of length 1 and needed to be escaped, then it would require 4 characters
                  // for the original, the escape, and the quote
                  if constexpr (detail::resizeable<B>) {
                     const auto k = ix + 4 * n;
                     if (k >= b.size()) [[unlikely]] {
                        b.resize((std::max)(b.size() * 2, k));
                     }
                  }
                  // now we don't have to check writing

                  dump_unchecked<'"'>(b, ix);

                  for (auto&& c : str) {
                     switch (c) {
                     case '"':
                        std::memcpy(data_ptr(b) + ix, R"(\")", 2);
                        ix += 2;
                        break;
                     case '\\':
                        std::memcpy(data_ptr(b) + ix, R"(\\)", 2);
                        ix += 2;
                        break;
                     case '\b':
                        std::memcpy(data_ptr(b) + ix, R"(\b)", 2);
                        ix += 2;
                        break;
                     case '\f':
                        std::memcpy(data_ptr(b) + ix, R"(\f)", 2);
                        ix += 2;
                        break;
                     case '\n':
                        std::memcpy(data_ptr(b) + ix, R"(\n)", 2);
                        ix += 2;
                        break;
                     case '\r':
                        std::memcpy(data_ptr(b) + ix, R"(\r)", 2);
                        ix += 2;
                        break;
                     case '\t':
                        std::memcpy(data_ptr(b) + ix, R"(\t)", 2);
                        ix += 2;
                        break;
                     [[likely]] default:
                        std::memcpy(data_ptr(b) + ix, &c, 1);
                        ++ix;
                     }
                  }

                  dump_unchecked<'"'>(b, ix);
               }
            }
         }
      };

      template <glaze_enum_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using key_t = std::underlying_type_t<T>;
            static constexpr auto frozen_map = detail::make_enum_to_string_map<T>();
            const auto& member_it = frozen_map.find(static_cast<key_t>(value));
            if (member_it != frozen_map.end()) {
               const sv str = {member_it->second.data(), member_it->second.size()};
               // Note: Assumes people dont use strings with chars that need to
               // be
               // escaped for their enum names
               // TODO: Could create a pre qouted map for better perf
               dump<'"'>(args...);
               dump(str, args...);
               dump<'"'>(args...);
            }
            else [[unlikely]] {
               // What do we want to happen if the value doesnt have a mapped
               // string
               write<json>::op<Opts>(static_cast<std::underlying_type_t<T>>(value), ctx, std::forward<Args>(args)...);
            }
         }
      };

      template <func_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            dump<'"'>(args...);
            dump(name_v<std::decay_t<decltype(value)>>, args...);
            dump<'"'>(args...);
         }
      };

      template <class T>
      struct to_json<basic_raw_json<T>>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) noexcept
         {
            dump(value.str, b, ix);
         }
      };

      template <array_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'['>(args...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            const auto is_empty = [&]() -> bool {
               if constexpr (has_size<T>) {
                  return value.size() ? false : true;
               }
               else {
                  return value.empty();
               }
            }();

            if (!is_empty) {
               auto it = value.begin();
               write<json>::op<Opts>(*it, ctx, args...);
               ++it;
               const auto end = value.end();
               for (; it != end; ++it) {
                  dump<','>(args...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(args...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
                  write<json>::op<Opts>(*it, ctx, args...);
               }
               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(args...);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            dump<']'>(args...);
         }
      };

      template <map_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'{'>(args...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            if (!value.empty()) {
               auto it = value.cbegin();
               auto write_pair = [&] {
                  using Key = decltype(it->first);
                  if constexpr (str_t<Key> || char_t<Key>) {
                     write<json>::op<Opts>(it->first, ctx, args...);
                     dump<':'>(args...);
                  }
                  else {
                     dump<'"'>(args...);
                     write<json>::op<Opts>(it->first, ctx, args...);
                     dump<R"(":)">(args...);
                  }
                  if constexpr (Opts.prettify) {
                     dump<' '>(args...);
                  }
                  write<json>::op<Opts>(it->second, ctx, args...);
               };
               write_pair();
               ++it;

               const auto end = value.cend();
               for (; it != end; ++it) {
                  using Value = std::decay_t<decltype(it->second)>;
                  if constexpr (null_t<Value> && Opts.skip_null_members) {
                     if (!bool(it->second)) continue;
                  }
                  dump<','>(args...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(args...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
                  write_pair();
               }
               if constexpr (Opts.prettify) {
                  ctx.indentation_level -= Opts.indentation_width;
                  dump<'\n'>(args...);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }
            dump<'}'>(args...);
         }
      };

      template <nullable_t T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if (value)
               write<json>::op<Opts>(*value, ctx, std::forward<Args>(args)...);
            else {
               dump<"null">(std::forward<Args>(args)...);
            }
         }
      };

      template <always_null_t T>
      struct to_json<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&&, is_context auto&&, auto&&... args) noexcept
         {
            dump<"null">(args...);
         }
      };

      template <is_variant T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            std::visit(
               [&](auto&& val) {
                  using V = std::decay_t<decltype(val)>;

                  if constexpr (Opts.write_type_info && !tag_v<T>.empty() && glaze_object_t<V>) {
                     // must first write out type
                     if constexpr (Opts.prettify) {
                        dump<"{\n">(args...);
                        ctx.indentation_level += Opts.indentation_width;
                        dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                        dump<'"'>(args...);
                        dump(tag_v<T>, args...);
                        dump<"\": \"">(args...);
                        dump(ids_v<T>[value.index()], args...);
                        dump<"\",\n">(args...);
                        dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                     }
                     else {
                        dump<"{\"">(args...);
                        dump(tag_v<T>, args...);
                        dump<"\":\"">(args...);
                        dump(ids_v<T>[value.index()], args...);
                        dump<R"(",)">(args...);
                     }
                     write<json>::op<opening_handled<Opts>()>(val, ctx, args...);
                  }
                  else {
                     write<json>::op<Opts>(val, ctx, args...);
                  }
               },
               value);
         }
      };

      template <class T>
      struct to_json<array_variant_wrapper<T>>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& wrapper, is_context auto&& ctx, Args&&... args) noexcept
         {
            auto& value = wrapper.value;
            dump<'['>(args...);
            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<'"'>(args...);
            dump(ids_v<T>[value.index()], args...);
            dump<"\",">(args...);
            if constexpr (Opts.prettify) {
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            std::visit([&](auto&& v) { write<json>::op<Opts>(v, ctx, args...); }, value);
            if constexpr (Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<']'>(args...);
         }
      };

      template <class T>
         requires is_specialization_v<T, arr>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = std::tuple_size_v<V>;

            dump<'['>(args...);
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(get_member(value.value, glz::tuplet::get<I>(meta_v<T>)), ctx, args...);
               }
               else {
                  write<json>::op<Opts>(glz::tuplet::get<I>(value.value), ctx, args...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(args...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(args...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<']'>(args...);
         }
      };

      template <class T>
         requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }();

            dump<'['>(args...);
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, args...);
               }
               else {
                  write<json>::op<Opts>(glz::tuplet::get<I>(value), ctx, args...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(args...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(args...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<']'>(args...);
         }
      };

      template <class T>
      struct to_json<includer<T>>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&& /*ctx*/, Args&&...) noexcept
         {}
      };

      template <class T>
         requires is_std_tuple<std::decay_t<T>>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            static constexpr auto N = []() constexpr {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }();

            dump<'['>(args...);
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(value.*std::get<I>(meta_v<V>), ctx, args...);
               }
               else {
                  write<json>::op<Opts>(std::get<I>(value), ctx, args...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_comma = I < N - 1;
               if constexpr (needs_comma) {
                  dump<','>(args...);
                  if constexpr (Opts.prettify) {
                     dump<'\n'>(args...);
                     dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
                  }
               }
            });
            if constexpr (N > 0 && Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<']'>(args...);
         }
      };

      template <const std::string_view& S>
      GLZ_ALWAYS_INLINE constexpr auto array_from_sv() noexcept
      {
         constexpr auto s = S; // Needed for MSVC to avoid an internal compiler error
         constexpr auto N = s.size();
         std::array<char, N> arr;
         std::copy_n(s.data(), N, arr.data());
         return arr;
      }

      GLZ_ALWAYS_INLINE constexpr bool needs_escaping(const auto& S) noexcept
      {
         for (const auto& c : S) {
            if (c == '"') {
               return true;
            }
         }
         return false;
      }

      template <class T>
         requires is_specialization_v<T, glz::obj>
      struct to_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            if constexpr (!Options.opening_handled) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            using V = std::decay_t<decltype(value.value)>;
            static constexpr auto N = std::tuple_size_v<V> / 2;

            bool first = true;
            for_each<N>([&](auto I) {
               static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
               decltype(auto) item = glz::tuplet::get<2 * I + 1>(value.value);
               using val_t = std::decay_t<decltype(item)>;

               if constexpr (null_t<val_t> && Opts.skip_null_members) {
                  if constexpr (always_null_t<T>)
                     return;
                  else {
                     auto is_null = [&]() { return !bool(item); }();
                     if (is_null) return;
                  }
               }

               // skip file_include
               if constexpr (std::is_same_v<val_t, includer<V>>) {
                  return;
               }
               else if constexpr (std::is_same_v<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  if (first) {
                     first = false;
                  }
                  else {
                     // Null members may be skipped so we cant just write it out for all but the last member unless
                     // trailing commas are allowed
                     dump<','>(b, ix);
                     if constexpr (Opts.prettify) {
                        dump<'\n'>(b, ix);
                        dumpn<Opts.indentation_char>(ctx.indentation_level, b, ix);
                     }
                  }

                  using Key = typename std::decay_t<std::tuple_element_t<2 * I, V>>;

                  if constexpr (str_t<Key> || char_t<Key>) {
                     const sv key = glz::tuplet::get<2 * I>(value.value);
                     write<json>::op<Opts>(key, ctx, b, ix);
                     dump<':'>(b, ix);
                     if constexpr (Opts.prettify) {
                        dump<' '>(b, ix);
                     }
                  }
                  else {
                     dump<'"'>(b, ix);
                     write<json>::op<Opts>(item, ctx, b, ix);
                     dump(Opts.prettify ? "\": " : "\":", b, ix);
                  }

                  write<json>::op<Opts>(item, ctx, b, ix);
               }
            });
            if constexpr (Options.prettify) {
               ctx.indentation_level -= Options.indentation_width;
               dump<'\n'>(b, ix);
               dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
            }
            dump<'}'>(b, ix);
         }
      };

      template <class T>
         requires glaze_object_t<T>
      struct to_json<T>
      {
         template <auto Options>
         GLZ_FLATTEN static void op(auto&& value, is_context auto&& ctx, auto&& b, auto&& ix) noexcept
         {
            if constexpr (!Options.opening_handled) {
               dump<'{'>(b, ix);
               if constexpr (Options.prettify) {
                  ctx.indentation_level += Options.indentation_width;
                  dump<'\n'>(b, ix);
                  dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
               }
            }

            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;

            bool first = true;
            for_each<N>([&](auto I) {
               static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               using mptr_t = std::tuple_element_t<1, decltype(item)>;
               using val_t = member_t<V, mptr_t>;

               if constexpr (null_t<val_t> && Opts.skip_null_members) {
                  if constexpr (always_null_t<T>)
                     return;
                  else {
                     auto is_null = [&]() {
                        if constexpr (std::is_member_pointer_v<std::tuple_element_t<1, decltype(item)>>) {
                           return !bool(value.*glz::tuplet::get<1>(item));
                        }
                        else {
                           return !bool(glz::tuplet::get<1>(item)(value));
                        }
                     }();
                     if (is_null) return;
                  }
               }

               // skip file_include
               if constexpr (std::is_same_v<val_t, includer<std::decay_t<V>>>) {
                  return;
               }
               else if constexpr (std::is_same_v<val_t, hidden> || std::same_as<val_t, skip>) {
                  return;
               }
               else {
                  if (first) {
                     first = false;
                  }
                  else {
                     // Null members may be skipped so we cant just write it out for all but the last member unless
                     // trailing commas are allowed
                     dump<','>(b, ix);
                     if constexpr (Opts.prettify) {
                        dump<'\n'>(b, ix);
                        dumpn<Opts.indentation_char>(ctx.indentation_level, b, ix);
                     }
                  }

                  using Key = typename std::decay_t<std::tuple_element_t<0, decltype(item)>>;

                  if constexpr (str_t<Key> || char_t<Key>) {
                     static constexpr sv key = glz::tuplet::get<0>(item);
                     if constexpr (needs_escaping(key)) {
                        write<json>::op<Opts>(key, ctx, b, ix);
                        dump<':'>(b, ix);
                        if constexpr (Opts.prettify) {
                           dump<' '>(b, ix);
                        }
                     }
                     else {
                        if constexpr (Opts.prettify) {
                           static constexpr auto quoted = join_v<chars<"\"">, key, chars<"\": ">>;
                           dump<quoted>(b, ix);
                        }
                        else {
                           static constexpr auto quoted = join_v<chars<"\"">, key, chars<"\":">>;
                           dump<quoted>(b, ix);
                        }
                     }
                  }
                  else {
                     static constexpr auto quoted =
                        concat_arrays(concat_arrays("\"", glz::tuplet::get<0>(item)), "\":", Opts.prettify ? " " : "");
                     write<json>::op<Opts>(quoted, ctx, b, ix);
                  }

                  write<json>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, b, ix);

                  static constexpr auto S = std::tuple_size_v<decltype(item)>;
                  if constexpr (Opts.comments && S > 2) {
                     static constexpr sv comment = glz::tuplet::get<2>(item);
                     if constexpr (comment.size() > 0) {
                        if constexpr (Opts.prettify) {
                           dump<' '>(b, ix);
                        }
                        dump<"/*">(b, ix);
                        dump(comment, b, ix);
                        dump<"*/">(b, ix);
                     }
                  }
               }
            });
            if constexpr (Options.prettify) {
               ctx.indentation_level -= Options.indentation_width;
               dump<'\n'>(b, ix);
               dumpn<Options.indentation_char>(ctx.indentation_level, b, ix);
            }
            dump<'}'>(b, ix);
         }
      };
   } // namespace detail

   template <class T, class Buffer>
   GLZ_ALWAYS_INLINE auto write_json(T&& value, Buffer&& buffer)
   {
      return write<opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   GLZ_ALWAYS_INLINE auto write_json(T&& value)
   {
      std::string buffer{};
      write<opts{}>(std::forward<T>(value), buffer);
      return buffer;
   }

   template <class T, class Buffer>
   GLZ_ALWAYS_INLINE void write_jsonc(T&& value, Buffer&& buffer)
   {
      write<opts{.comments = true}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   GLZ_ALWAYS_INLINE auto write_jsonc(T&& value)
   {
      std::string buffer{};
      write<opts{.comments = true}>(std::forward<T>(value), buffer);
      return buffer;
   }

   // std::string file_name needed for std::ofstream
   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE write_error write_file_json(T&& value, const std::string& file_name,
                                                               auto&& buffer) noexcept
   {
      write<opts{}>(std::forward<T>(value), buffer);
      return {buffer_to_file(buffer, file_name)};
   }

   template <class T>
   [[deprecated(
      "use the version that takes a buffer as the third argument")]] [[nodiscard]] GLZ_ALWAYS_INLINE write_error
   write_file_json(T&& value, const std::string& file_name) noexcept
   {
      std::string buffer{};
      return write_file_json(std::forward<T>(value), file_name, buffer);
   }
}
