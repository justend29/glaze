#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#ifdef GLAZE_BUILD_TESTING
#include <map>
#include <vector>
#endif

#include "glaze/core/common.hpp" // custom_write & custom_read
#include "glaze/core/meta.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   namespace detail
   {
      template <typename T>
      concept has_make_null_member = requires() {
         {
            T::make_null()
         } -> std::convertible_to<T>;
      };

      template <typename T>
      concept has_is_null_member = requires(const T t) {
         {
            t.is_null()
         } -> std::constructible_from<bool>;
      };

      template <typename T>
      concept has_make_undefined_member = requires() {
         {
            T::make_undefined()
         } -> std::convertible_to<T>;
      };

      template <typename T>
      concept has_is_undefined_member = requires(const T t) {
         {
            t.is_undefined()
         } -> std::constructible_from<bool>;
      };

      template <typename T>
      concept has_value_member = requires(T t) {
         !std::is_const_v<decltype(t.value())>;
         std::is_lvalue_reference_v<decltype(t.value())>;
      };

      template <typename T>
      concept has_const_value_member = requires(const T t) {
         std::is_const_v<decltype(t.value())>;
         std::is_lvalue_reference_v<decltype(t.value())>;
      } || requires(const T t) { !std::is_reference_v<decltype(t.value())>; };

      template <typename T>
      concept has_make_for_overwrite_member = requires {
         {
            T::make_for_overwrite()
         } -> std::convertible_to<T>;
      };

      template <typename T>
      concept known_nullable_container =
         is_specialization_v<T, std::optional> || is_specialization_v<T, std::unique_ptr> ||
         is_specialization_v<T, std::shared_ptr>;

      template <typename T>
      [[nodiscard]] constexpr auto make_known_nullable_for_overwrite() noexcept
      {
         if constexpr (is_specialization_v<T, std::optional>) {
            return std::make_optional<typename T::value_type>();
         }
         else if constexpr (is_specialization_v<T, std::unique_ptr>) {
            return std::make_unique_for_overwrite<typename T::element_type>();
         }
         else {
            return std::make_shared_for_overwrite<typename T::element_type>();
         }
      }
   }

   // nully_interface is the uniform interface through which glaze will interact with nully types.
   // The nullability/undefinability of a type is governed by its level of conformance with the nully_interface.
   // This generic nully_interface template offers definitions for:
   // - well-known nully types
   // - types with well-known interfaces
   // - types with member functions used to satisfy this interface

   // where the default behaviour is a container presenting null/value and possibly undefinable
   // provide some sensible defaults for types with necessary member functions based on common standard C++ types
   // specializations can be made for custom types that don't offer supported default members, or for alternate
   // behaviour, as is done for undefinable below.
   // In addition to providing functions
   template <typename T>
   struct nully_interface
   {
      [[nodiscard]] static constexpr T make_null() noexcept
         requires detail::has_make_null_member<T> || std::is_default_constructible_v<T> ||
                  std::constructible_from<T, std::nullopt_t>
      {
         // move assigning a 'null' value nullifies source object for all types, including pointers
         if constexpr (detail::has_make_null_member<T>) {
            return T::make_null();
         }
         else if constexpr (std::constructible_from<T, std::nullopt_t>) {
            // std::nullopt_t is purposefully not default-constructible
            // additionally allows nully containers w/ std::nullopt_t constructors to be used
            return std::nullopt;
         }
         else {
            return {};
         }
      }

      [[nodiscard]] static constexpr bool is_null(const T& v) noexcept
         requires detail::has_is_null_member<T> ||
                  (std::constructible_from<bool, T> && !std::same_as<std::remove_cvref_t<T>, bool>)
      {
         if constexpr (detail::has_is_null_member<T>) {
            return T::is_null();
         }
         else {
            return !static_cast<bool>(v);
         }
      }

      // neither std::monostate nor std::nullopt_t can be converted to bool
      [[nodiscard]] static constexpr bool is_null(std::monostate) noexcept { return true; }
      [[nodiscard]] static constexpr bool is_null(std::nullopt_t) noexcept { return true; }

      [[nodiscard]] static constexpr T make_undefined() noexcept
         requires detail::has_make_undefined_member<T>
      {
         return T::make_undefined();
      }

      [[nodiscard]] static constexpr bool is_undefined(const T& v) noexcept
         requires detail::has_is_undefined_member<T>
      {
         return v.is_undefined();
      }

      [[nodiscard]] static constexpr const auto& value(const T& v) noexcept
         requires detail::has_const_value_member<T> || requires(const T t) {
            std::is_const_v<decltype(*t)>;
            std::is_lvalue_reference_v<decltype(*t)>;
         }
      {
         if constexpr (detail::has_const_value_member<T>) {
            return v.value();
         }
         else {
            return *v;
         }
      }

      [[nodiscard]] static constexpr auto& value(T& v) noexcept
         requires detail::has_value_member<T> || requires(const T t) {
            !std::is_const_v<decltype(*t)>;
            std::is_lvalue_reference_v<decltype(*t)>;
         }
      {
         if constexpr (detail::has_value_member<T>) {
            return v.value();
         }
         else {
            return *v;
         }
      }

      // Literal types don't enclose a value like nully containers. Instead, they are their literal value
      [[nodiscard]] static constexpr std::nullptr_t value(std::nullptr_t) noexcept { return nullptr; }
      [[nodiscard]] static constexpr std::nullopt_t value(std::nullopt_t) noexcept { return std::nullopt; }
      [[nodiscard]] static constexpr std::monostate value(std::monostate) noexcept { return {}; }
      [[nodiscard]] static constexpr std::false_type value(std::false_type) noexcept { return {}; }

      [[nodiscard]] static constexpr T make_for_overwrite() noexcept
         requires detail::has_make_for_overwrite_member<T>
      {
         return T::make_for_overwrite();
      }

      [[nodiscard]] static constexpr T make_for_overwrite() noexcept
         requires detail::known_nullable_container<T>
      {
         return detail::make_known_nullable_for_overwrite<T>();
      }
   };

   template <typename T>
      requires std::is_pointer_v<std::decay_t<T>>
   struct nully_interface<T>
   {};

   template <typename T>
   struct nully_value_type_impl
   {};

   template <std::constructible_from T>
      requires requires(const T t) { nully_interface<T>::value(t); }
   struct nully_value_type_impl<T>
   {
      //      using value_type = std::remove_cvref_t<decltype(nully_interface<T>::value(T{}))>;
      using value_type = std::remove_cvref_t<int>;
   };

   // nully_traits provides the type-traits available to query the nullability/undefinability of a given type.
   // The traits are contained within a struct to contain related traits, avoid name conflict, and clarify purpose of
   // each trait
   template <typename T>
   struct nully_traits : public nully_value_type_impl<T>
   {
      static constexpr bool can_make_null = requires { nully_interface<T>::make_null(); };
      static constexpr bool can_check_null = requires(const T t) { nully_interface<T>::is_null(t); };
      static constexpr bool can_make_undefined = requires { nully_interface<T>::make_undefined(); };
      static constexpr bool can_check_undefined = requires(const T t) { nully_interface<T>::is_undefined(t); };
      static constexpr bool can_get_value = requires(T t) { nully_interface<T>::value(t); };
      static constexpr bool can_get_mut_value = requires(T t) {
         {
            nully_interface<T>::value(t)
         } -> mut;
         {
            nully_interface<T>::value(t)
         } -> lv_reference;
      };
      static constexpr bool can_make_for_overwrite = requires { nully_interface<T>::make_for_overwrite(); };
   };

#ifdef __cpp_if_consteval
   template <typename T>
      requires nully_traits<T>::can_make_null && nully_traits<T>::can_check_null
   [[nodiscard]] constexpr bool is_always_null()
   {
      auto null = nully_interface<T>::make_null();
      [[maybe_unused]] const auto is_null = nully_interface<T>::is_null(null);
      if constexpr {
         return true;
      }
      else {
         return false;
      }
      //      return std::is_constant_evaluated();
   }

   template <typename T>
   inline constexpr bool is_always_null_v = requires { is_always_null<T>(); };

   template <typename T>
   concept always_null_t = is_always_null_v<T>;

#ifdef GLAZE_BUILD_TESTING
   namespace detail::type_validation
   {
      static_assert(always_null_t<std::nullptr_t>);
      static_assert(always_null_t<std::monostate>);
      static_assert(always_null_t<std::nullopt_t>);
      static_assert(always_null_t<std::false_type>);

      static_assert(!always_null_t<std::unique_ptr<int>>);
      static_assert(!always_null_t<std::shared_ptr<int>>);
      static_assert(!always_null_t<std::optional<int>>);
   }
#endif
#else
   template <class T>
   concept always_null_t = is_any_of<T, std::nullptr_t, std::monostate, std::nullopt_t>;
#endif

   //   template <class T>
   //   concept raw_nullable = is_specialization_v<T, raw_t> && requires { requires nullable_t<typename T::value_type>;
   //   };

   template <typename T>
   concept writable_nullable_t = !custom_write<T> && nully_traits<T>::can_check_null && nully_traits<T>::can_get_value;

   template <typename T>
   concept readable_nullable_t = !custom_read<T> && nully_traits<T>::can_check_null && nully_traits<T>::can_make_null &&
                                 nully_traits<T>::can_make_for_overwrite && nully_traits<T>::can_get_mut_value;

   template <typename T>
   concept writable_undefinable_t =
      !custom_write<T> && nully_traits<T>::can_check_undefined && nully_traits<T>::can_get_value;

   template <typename T>
   concept readable_undefinable_t =
      !custom_read<T> && nully_traits<T>::can_check_undefined && nully_traits<T>::can_make_undefined &&
      nully_traits<T>::can_make_for_overwrite && nully_traits<T>::can_get_mut_value;

   template <class T>
   concept nullable_t =
      readable_nullable_t<T> || writable_nullable_t<T> || always_null_t<T>; /** || raw_nullable<T>; **/

   template <class T>
   concept undefinable_t = readable_undefinable_t<T> || writable_undefinable_t<T>;

   struct null_tag
   {};
   struct undefined_tag
   {};

   // Tri-state object wrapper representing either undefined, null, or a value.
   template <std::constructible_from T>
   class nully : public std::variant<T, null_tag, undefined_tag>
   {
     public:
      using value_type = T;
      using std::variant<T, null_tag, undefined_tag>::variant;

      explicit operator bool() const noexcept { return std::holds_alternative<T>(*this); }
      [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<null_tag>(*this); }
      [[nodiscard]] bool is_undefined() const noexcept { return std::holds_alternative<undefined_tag>(*this); }
      [[nodiscard]] const T& value() const noexcept { return std::get<T>(*this); }
      [[nodiscard]] T& value() noexcept { return std::get<T>(*this); }
      [[nodiscard]] static nully<T> make_for_overwrite() noexcept { return {}; }
   };

   template <typename T>
      requires std::constructible_from<T>
   class nullable : private std::optional<T>
   {
      using value_type = std::optional<T>::value_type;
      using reference = std::optional<T>::reference;
      using const_reference = std::optional<T>::const_reference;
      using std::optional<T>::optional; // inherit constructors
      using std::optional<T>::operator bool;

      // provide member functions aligned with nullable interface rather than an optional interface
      [[nodiscard]] constexpr reference value() noexcept { return std::optional<T>::value(); }
      [[nodiscard]] constexpr const_reference value() const noexcept { return std::optional<T>::value(); }
      [[nodiscard]] constexpr bool is_null() const noexcept { return !std::optional<T>::has_value(); }
      [[nodiscard]] static nullable make_null() noexcept { return {}; }
      [[nodiscard]] static nullable make_for_overwrite() noexcept { return std::make_optional<T>(); };
   }

   template <typename T>
      requires std::constructible_from<T>
   class undefinable : public std::optional<T>
   {
     public:
      using value_type = std::optional<T>::value_type;
      using reference = std::optional<T>::reference;
      using const_reference = std::optional<T>::const_reference;
      using std::optional<T>::optional;
      using std::optional<T>::operator bool;

      // provide member functions aligned with undefinable rather than an optional
      [[nodiscard]] constexpr reference value() noexcept { return std::optional<T>::value(); }
      [[nodiscard]] constexpr const_reference value() const noexcept { return std::optional<T>::value(); }
      [[nodiscard]] constexpr bool is_undefined() const noexcept { return !std::optional<T>::has_value(); }
      [[nodiscard]] static undefinable make_undefined() noexcept { return {}; }
      [[nodiscard]] static undefinable make_for_overwrite() noexcept { return std::make_optional<T>(); };
   };

   template <typename T>
   struct nully_interface<undefinable<T>>
   {
      // specializing nully_interface instead of relying on generic nully_interface template to avoid is_null(),
      // since std::optional acts as nullable instead of undefinable by default.
      using Undefinable = undefinable<T>;

      [[nodiscard]] static Undefinable make_undefined() noexcept { return {}; }
      [[nodiscard]] static bool is_undefined(const Undefinable& v) noexcept { return !v->has_value(); }
      [[nodiscard]] static const T& value(const Undefinable& v) noexcept { return v.value(); };
      [[nodiscard]] static T& value(Undefinable& v) noexcept { return v.value(); };
      [[nodiscard]] static undefinable<T> make_for_overwrite() noexcept { return std::make_optional<T>(); }
   };

   // Nested nully types, where the composed types are exclusively nullable or undefinable, can act as both
   // undefinable and nullable. The following combinations of nested nully types act this way in the following forms:
   // - exclusively_undefinable<exclusively_nullable> in the form exclusively_undefinable<exclusively_nullable>
   // - exclusively_nullable<exclusively_nullable> in the form exclusively_undefinable<exclusively_nullable>
   // - exclusively_undefinable<exclusively_nullable> exclusively_nullable<exclusively_nullable> => acts as =>
   // - exclusively_undefinable<exclusively_nullable>
   // However, the nully_interface for nested types cannot be implemented generically in terms of the outer type, or
   // recursion will ensue

   template <typename T>
      requires detail::known_nullable_container<T> && nullable_t<typename nully_traits<T>::value_type> &&
               requires() { !undefinable_t<typename nully_traits<T>::value_type>; }
   struct nully_interface<T>
   {
      using Inner = typename nully_traits<T>::value_type;
      [[nodiscard]] static constexpr T make_undefined() noexcept { return {}; }
      [[nodiscard]] static constexpr bool is_undefined(const T& v) noexcept { return !static_cast<bool>(v); }

      [[nodiscard]] static constexpr T make_null() noexcept
         requires nully_traits<Inner>::can_make_null && std::constructible_from<T, std::remove_reference_t<Inner>&&>
      {
         return T{nully_interface<Inner>::make_null()};
      }

      [[nodiscard]] static constexpr bool is_null(const T& v) noexcept
      {
         return !is_undefined(v) && (always_null_t<Inner> || nully_interface<Inner>::is_null());
      }

      [[nodiscard]] static constexpr const auto& value(const T& v) noexcept { return nully_traits<Inner>::value(*v); }
      [[nodiscard]] static constexpr auto& value(T& v) noexcept
         requires nully_traits<Inner>::can_get_mut_value
      {
         return nully_interface<Inner>::value(*v);
      }
      [[nodiscard]] static T make_for_overwrite() noexcept { return detail::make_known_nullable_for_overwrite<T>(); }
   };

   template <typename T>
      requires nullable_t<typename nully_traits<T>::value_type> &&
               requires() { !undefinable_t<typename nully_traits<T>::value_type>; }
   struct nully_interface<undefinable<T>>
   {
      using Undefinable = undefinable<T>;
      [[nodiscard]] static constexpr Undefinable make_undefined() noexcept { return Undefinable::make_undefined(); }
      [[nodiscard]] static constexpr bool is_undefined(const Undefinable& v) noexcept { return v.is_undefined(); }

      [[nodiscard]] static constexpr T make_null() noexcept
         requires nully_traits<T>::can_make_null
      {
         return Undefinable{nully_interface<T>::make_null()};
      }

      [[nodiscard]] static constexpr bool is_null(const Undefinable& v) noexcept
      {
         return !is_undefined(v) && (always_null_t<T> || nully_interface<T>::is_null());
      }

      [[nodiscard]] static constexpr const auto& value(const Undefinable& v) noexcept
      {
         return nully_traits<T>::value(*v);
      }

      [[nodiscard]] static constexpr auto& value(T& v) noexcept
         requires nully_traits<T>::can_get_mut_value
      {
         return nully_interface<T>::value(*v);
      }
      [[nodiscard]] static T make_for_overwrite() noexcept { return Undefinable::make_for_overwrite(); }
   };

#ifdef GLAZE_BUILD_TESTING
   namespace detail::type_validation
   {
      using Type1 = int;
      using Type2 = std::vector<std::vector<double>>;
      using Type3 = std::map<int, std::vector<const char*>>;

      template <template <class> class Constraint>
      consteval bool assert_nullable_and_undefinable()
      {
         static_assert(readable_nullable_t<Constraint<Type1>>);
         static_assert(writable_nullable_t<Constraint<Type1>>);
         static_assert(readable_nullable_t<Constraint<Type2>>);
         static_assert(writable_nullable_t<Constraint<Type2>>);
         static_assert(readable_nullable_t<Constraint<Type3>>);
         static_assert(writable_nullable_t<Constraint<Type3>>);
         return true;
      }

      template <template <class> class TType>
      consteval bool assert_exclusively_nullable()
      {
         static_assert(readable_nullable_t<TType<Type1>>);
         static_assert(writable_nullable_t<TType<Type1>>);
         static_assert(readable_nullable_t<TType<Type2>>);
         static_assert(writable_nullable_t<TType<Type2>>);
         static_assert(readable_nullable_t<TType<Type3>>);
         static_assert(writable_nullable_t<TType<Type3>>);
         static_assert(!readable_undefinable_t<TType<Type1>>);
         static_assert(!writable_undefinable_t<TType<Type1>>);
         static_assert(!readable_undefinable_t<TType<Type2>>);
         static_assert(!writable_undefinable_t<TType<Type2>>);
         static_assert(!readable_undefinable_t<TType<Type3>>);
         static_assert(!writable_undefinable_t<TType<Type3>>);
         return true;
      }

      template <template <class> class TType>
      consteval bool assert_exclusively_undefinable()
      {
         static_assert(readable_undefinable_t<TType<Type1>>);
         static_assert(writable_undefinable_t<TType<Type1>>);
         static_assert(readable_undefinable_t<TType<Type2>>);
         static_assert(writable_undefinable_t<TType<Type2>>);
         static_assert(readable_undefinable_t<TType<Type3>>);
         static_assert(writable_undefinable_t<TType<Type3>>);
         static_assert(!readable_nullable_t<TType<Type1>>);
         static_assert(!writable_nullable_t<TType<Type1>>);
         static_assert(!readable_nullable_t<TType<Type2>>);
         static_assert(!writable_nullable_t<TType<Type2>>);
         static_assert(!readable_nullable_t<TType<Type3>>);
         static_assert(!writable_nullable_t<TType<Type3>>);
         return true;
      }

      static_assert(assert_nullable_and_undefinable<nully>());
      static_assert(assert_exclusively_undefinable<undefinable>());
      static_assert(assert_exclusively_nullable<std::optional>());
      static_assert(assert_exclusively_nullable<std::unique_ptr>());
      static_assert(assert_exclusively_nullable<std::shared_ptr>());

      static_assert(writable_nullable_t<std::nullptr_t>);
      static_assert(writable_nullable_t<std::nullopt_t>);
      static_assert(writable_nullable_t<std::monostate>);
      static_assert(writable_nullable_t<std::false_type>);
      static_assert(!readable_nullable_t<std::nullptr_t>);
      static_assert(!readable_nullable_t<std::nullopt_t>);
      static_assert(!readable_nullable_t<std::monostate>);
      static_assert(!readable_nullable_t<std::false_type>);
   }
#endif
}
