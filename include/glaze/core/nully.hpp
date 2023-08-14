#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include "glaze/core/meta.hpp"
#include "glaze/util/type_traits.hpp"

namespace glz
{
   namespace detail
   {
      template <typename T>
      concept Has_make_null_member = requires() {
         {
            T::make_null()
         } -> std::convertible_to<T>;
      };

      template <typename T>
      concept Has_is_null_member = requires(const T t) {
         {
            t.is_null()
         } -> std::constructible_from<bool>;
      };

      template <typename T>
      concept Has_make_undefined_member = requires() {
         {
            T::make_undefined()
         } -> std::convertible_to<T>;
      };

      template <typename T>
      concept Has_is_undefined_member = requires(const T t) {
         {
            t.is_undefined()
         } -> std::constructible_from<bool>;
      };

      template <typename T>
      concept Has_value_member = requires(T t) {
         !std::is_const_v<decltype(t.value())>;
         std::is_lvalue_reference_v<decltype(t.value())>;
      };

      template <typename T>
      concept Has_const_value_member = requires(T t) {
         std::is_const_v<decltype(t.value())>;
         std::is_lvalue_reference_v<decltype(t.value())>;
      };

      template <typename T>
      concept Has_make_for_overwrite_member = requires {
         {
            T::make_for_overwrite()
         } -> std::convertible_to<T>;
      };
   }

   // TODO: nested nulls turn into undefined, then null

   template <typename T>
   concept Bistate = true; // boolean testible/boolean convertible

   // where the default behaviour is a container presenting null/value and possibly undefinable
   // provide some sensible defaults for types with necessary member functions based on common standard C++ types
   // specializations can be made for custom types that don't offer supported default members, or for alternate
   // behaviour, as is done for undefinable below.
   // In addition to providing functions
   template <typename T>
   struct nully_interface
   {
      [[nodiscard]] static constexpr T make_null() noexcept
         requires detail::Has_make_null_member<T> || std::is_default_constructible_v<T> ||
                  std::constructible_from<T, std::nullopt_t>
      {
         // move assigning a default constructed value 'resets' all types, including pointers
         if constexpr (detail::Has_make_null_member<T>) {
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
         requires detail::Has_is_null_member<T> || std::constructible_from<bool, T>
      {
         // requires operator bool
         if constexpr (detail::Has_is_null_member<T>) {
            return T::is_null();
         }
         else {
            return !static_cast<bool>(v);
         }
      }

      // neither std::monostate nor std::nullopt_t can be converted to bool
      [[nodiscard]] static constexpr bool is_null(const std::monostate& v) noexcept { return true; }
      [[nodiscard]] static constexpr bool is_null(const std::nullopt_t& v) noexcept { return true; }

      [[nodiscard]] static constexpr T make_undefined() noexcept
         requires detail::Has_make_undefined_member<T>
      {
         return T::make_undefined();
      }

      [[nodiscard]] static constexpr bool is_undefined(const T& v) noexcept
         requires detail::Has_is_undefined_member<T>
      {
         return v.is_undefined();
      }

      [[nodiscard]] static constexpr const auto& value(const T& v) noexcept
         requires detail::Has_const_value_member<T> || requires(const T t) {
            std::is_const_v<decltype(*t)>;
            std::is_lvalue_reference_v<decltype(*t)>;
         }
      {
         if constexpr (detail::Has_const_value_member<T>) {
            return v.value();
         }
         else {
            return *v;
         }
      }

      [[nodiscard]] static constexpr auto& value(T& v) noexcept
         requires detail::Has_value_member<T> || requires(const T t) {
            !std::is_const_v<decltype(*t)>;
            std::is_lvalue_reference_v<decltype(*t)>;
         }
      {
         if constexpr (detail::Has_value_member<T>) {
            return v.value();
         }
         else {
            return *v;
         }
      }

      [[nodiscard]] static constexpr T make_for_overwrite() noexcept
      {
         if constexpr (detail::Has_make_for_overwrite_member<T>) {
            return T::make_for_overwrite();
         }
         else if constexpr (is_specialization_v<T, std::optional>) {
            return std::make_optional<typename T::value_type>();
         }
         else if constexpr (is_specialization_v<T, std::unique_ptr>) {
            return std::make_unique_for_overwrite<typename T::element_type>();
         }
         else if constexpr (is_specialization_v<T, std::shared_ptr>) {
            return std::make_shared_for_overwrite<typename T::element_type>();
         }
         else {
            static_assert(false_v<>, "specialize type and provide construction");
         }
      }
   };

   template <typename T>
   struct nully_value_type_impl
   {};

   template <typename T>
      requires requires(const T t) { nully_interface<T>::value(t); }
   struct nully_value_type_impl<T>
   {
      using value_type =
         std::remove_cvref_t<std::invoke_result_t<typename nully_interface<T>::value, decltype(std::declval<T>)>>;
   };

   // within traits struct to contain related traits, avoid name conflict, and clarify purpose of each trait
   // apply to any specialization of nully_interface or overloads of methods therein, not just the generic one provided
   // w/ glaze
   // TODO: is this a collection of 'traits'?
   template <typename T>
   struct nully_traits : public nully_value_type_impl<T>
   {
      static constexpr bool can_make_null = requires { nully_interface<T>::make_null(); };
      static constexpr bool can_check_null = requires(const T t) { nully_interface<T>::is_null(t); };
      static constexpr bool can_make_undefined = requires { nully_interface<T>::make_undefined(); };
      static constexpr bool can_check_undefined = requires(const T t) { nully_interface<T>::is_undefined(t); };
      static constexpr bool can_get_value = requires(T t) { nully_interface<T>::value(t); };
      static constexpr bool can_get_const_value = requires(const T t) { nully_interface<T>::value(t); };
      static constexpr bool can_get_mut_value = requires(const T t) {
         {
            nully_interface<T>::value(t)
         } -> mut;
      };
      static constexpr bool can_make_for_overwrite = requires { nully_interface<T>::make_for_overwrite(); };
   };

#ifdef __cpp_lib_is_constant_evaluated
   template <typename T>
      requires nully_traits<T>::can_make_null && nully_traits<T>::can_check_null
   [[nodiscard]] constexpr bool is_always_null()
   {
      const auto null = nully_interface<T>::make_null();
      [[maybe_unused]] const auto is_null = nully_interface<T>::is_null(null);
      if (std::is_constant_evaluated()) {
         return true;
      }
      else {
         return false;
      }
   }

   template <typename T>
   inline constexpr auto is_always_null_v = is_always_null<T>();

   template <typename T>
   concept always_null_t = is_always_null_v<T>;

//         template <class T>
//         concept raw_nullable = is_specialization_v<T, raw_t> && requires { requires nullable_t<typename
//         T::value_type>; };

   // TODO: add a shitload of static asserts surrounded by build_tests check
   namespace detail::std_compatibility_validation
   {
      static_assert(always_null_t<std::nullptr_t>);
      static_assert(always_null_t<std::monostate>);
      static_assert(always_null_t<std::nullopt_t>);
      static_assert(always_null_t<std::false_type>);
   }
#else
   template <class T>
   concept always_null_t =
      std::same_as<T, std::nullptr_t> || std::same_as<T, std::monostate> || std::same_as<T, std::nullopt_t>;
#endif

   template <typename T>
   concept writable_nullable_t = nully_traits<T>::can_check_null && nully_traits<T>::can_get_value;

   // TODO: ensure read value result type is not const
   template <typename T>
   concept readable_nullable_t = nully_traits<T>::can_check_null && nully_traits<T>::can_make_null &&
                                 nully_traits<T>::can_make_for_overwrite && nully_traits<T>::can_get_value;

   template <typename T>
   concept writable_undefinable_t = nully_traits<T>::can_check_undefined && nully_traits<T>::can_get_value;

   // TODO: ensure read value result type is not const
   template <typename T>
   concept readable_undefinable_t = nully_traits<T>::can_check_undefined && nully_traits<T>::can_make_undefined &&
                                    nully_traits<T>::can_make_for_overwrite && nully_traits<T>::can_get_value;

   template <class T>
   concept null_t = readable_nullable_t<T> || writable_nullable_t<T> || always_null_t<T>; /** || raw_nullable<T>; **/

   struct null_tag
   {};
   struct undefined_tag
   {};

   // Tri-state object wrapper representing undefined, null, or a value.
   template <std::constructible_from T>
   class nully : public std::variant<T, null_tag, undefined_tag>
   {
      using value_type = T;
      explicit operator bool() { return std::holds_alternative<T>(*this); }

      [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<null_tag>(*this); }
      [[nodiscard]] bool is_undefined() const noexcept { return std::holds_alternative<undefined_tag>(*this); }
      [[nodiscard]] const T& value() const noexcept { return std::get<T>(*this); }
      [[nodiscard]] T& value() noexcept { return std::get<T>(*this); }
      [[nodiscard]] static nully<T> make_for_overwrite() noexcept { return {}; }
   };

   template <typename T>
   class undefinable : public std::optional<T>
   {
      using value_type = T;
      // optional provides operator bool(), value(), operator*()
      [[nodiscard]] static undefinable<T> make_undefined() { return {}; }
      [[nodiscard]] bool is_undefined() const noexcept { return !this->has_value(); }
      [[nodiscard]] static nully<T> make_for_overwrite() noexcept { return std::make_optional<T>(); }
   };

   template <typename T>
   struct nully_interface<undefinable<T>>
   {
      using undefinable_t = undefinable<T>;
      [[nodiscard]] static constexpr undefinable_t make_undefined() noexcept { return undefinable_t::make_undefined(); }
      [[nodiscard]] static bool is_undefined(const undefinable_t& v) noexcept { return v.is_undefined(); }
      [[nodiscard]] static constexpr const T& value(const undefinable_t& v) noexcept { return *v; }
      [[nodiscard]] static constexpr T& value(undefinable_t& v) noexcept { return *v; }
      [[nodiscard]] static undefinable_t make_for_overwrite() noexcept { return undefinable_t::make_for_overwrite(); }
   };

   namespace detail::std_compatibility_validation
   {
      // TODO: add a shitload of static asserts surrounded by build_tests check
      static_assert(always_null_t<std::nullptr_t>);
      static_assert(always_null_t<std::monostate>);
      static_assert(always_null_t<std::nullopt_t>);
   }
}
