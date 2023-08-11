#pragma once

namespace glz
{

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
         // requires dereferenceable
         return *v;
      }
   };

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
