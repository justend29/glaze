#pragma once

#include <type_traits>

namespace glz
{
   // TODO: replace nullable_t concept
   // TODO: replace always_null_t concept
   // TODO: replace null_t concept
   // TODO: create read & write nullable concepts
   // TODO: could have each function as a free function
   // TODO: consider system to indicate default constructed is null to avoid assignment in making null

   // provide some sensical defaults for values based on common standard C++ types; those that act like a pointer
   template <typename T>
   struct null_traits
   {
      [[nodiscard]] static T make_null() noexcept
      {
         // does 'make_null' conflict with construct?
         // using name 'construct' from meta_construct_v<> - what was the purpose of this?
         // requires default constructible
         return {};
      }

      [[nodiscard]] static bool is_null(const T& v) noexcept
      {
         // requires operator bool
         return !static_cast<bool>(v);
      }

      [[nodiscard]] static auto& value(const T& v) noexcept
      {
         // specifically named value to conflict with glaze'd objects 'value' member
         // requires dereferenceable
         return *v;
      }
   };

   template <typename T>
   concept can_make_null = std::is_invocable_r_v<T, decltype(::glz::null_traits<T>::make_null)>;

   template <typename T>
   concept can_check_null = std::is_invocable_r_v<T, decltype(::glz::null_traits<T>::is_null)>;

   template <typename T>
   concept can_get_null_value = std::is_invocable_r_v<T, decltype(::glz::null_traits<T>::value)>;

   // provide some sensical defaults for values based on common standard C++ types; those that act like a pointer
   template <typename T>
   struct undefined_traits
   {
      [[nodiscard]] static T make_undefined() noexcept
      {
         // does 'make_null' conflict with construct?
         // using name 'construct' from meta_construct_v<> - what was the purpose of this?
         // requires default constructible
         return {};
      }

      [[nodiscard]] static bool is_undefined(const T& v) noexcept
      {
         // requires operator bool
         return !static_cast<bool>(v);
      }

      [[nodiscard]] static auto& value(const T& v) noexcept
      {
         // requires dereferenceable
         return *v;
      }
   };

}
