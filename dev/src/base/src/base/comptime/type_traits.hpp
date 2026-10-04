/**
 * @file type_traits.hpp
 *
 * @brief Type traits provides a set of functionalities useful during metaprogramming with types.
 *
 * Functionalities
 * ===============
 *
 * Concepts:
 * ---------
 * - base::IsInstantiationOf
 * - base::IsNumber
 * - base::IsOfSameClass
 * - base::IsVariantMember
 * - base::IsVariant
 *
 * Functions:
 * ----------
 * - base::typeName
 * - base::variantTypeIndex
 *
 * Variables:
 * ----------
 * - base::Implication
 *
 * Type Traits:
 * ------------
 * - base::is_variant_member
 * - base::IS_VARIANT_MEMBER_V
 *
 * ### Usage
 * @include type_traits_example.cpp
 *
 * @example type_traits_example.cpp
 */
#pragma once

#include <base/config/target_info.hpp>
#include <base/types/ints.hpp>

#include <limits>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

namespace base {
	namespace internal {
		template<class, class>
		struct IsOfSameClassImpl: public std::false_type {};

		template<class T1, class T2, template<class> class U>
		struct IsOfSameClassImpl<U<T1>, U<T2>>: public std::true_type {};

		template<typename Tup>
		struct VariantHoldsOneOfImpl;

		template<typename... Ts>
		struct VariantHoldsOneOfImpl<std::tuple<Ts...>> {
			template<typename... VariantArgs>
			constexpr bool operator()(const std::variant<VariantArgs...>& v) const {
				return (std::holds_alternative<Ts>(v) || ...);
			}
		};

		// Source - https://stackoverflow.com/a/11251376/
		// {
		template<typename T, template<typename...> typename Template>
		inline constexpr bool IS_INSTANTIATION_OF_V = false;

		template<template<typename...> typename Template, typename... Args>
		inline constexpr bool IS_INSTANTIATION_OF_V<Template<Args...>, Template> = true;

		template<typename T, template<typename, auto> class Template>
		inline constexpr bool IS_INSTANTIATION_OF_TYPE_VALUE_V = false;

		template<template<typename, auto> class Template, typename U, auto V>
		inline constexpr bool IS_INSTANTIATION_OF_TYPE_VALUE_V<Template<U, V>, Template> = true;

		// }

		// Source - https://stackoverflow.com/a/52303687
		// {

		template<typename...>
		inline constexpr bool DEPENDENT_FALSE_V = false;

		template<typename>
		struct Tag {};

		template<typename Variant, typename T>
		struct VariantTypeIndexAux {
			static_assert(
				DEPENDENT_FALSE_V<Variant>, "variantTypeIndex() can be used only for variant"
			);
		};

		template<typename T, typename... Types>
		struct VariantTypeIndexAux<std::variant<Types...>, T> {
			static constexpr usize findIndex() {
				return std::variant<Tag<Types>...>(Tag<T>{}).index();
			}
		};

		// }
	}

	/**
	 * @brief Always false, but only once a template is instantiated.
	 * @details The way to write a `static_assert` in the `else` of an `if constexpr` chain that
	 * fires for the types that reach it and not while the template is merely parsed.
	 */
	template<typename... Ts>
	inline constexpr bool DEPENDENT_FALSE_V = internal::DEPENDENT_FALSE_V<Ts...>;

	/**
	 * @brief Carries a type as a value, for passing one to a function or through overload
	 * resolution without ever instantiating it.
	 */
	template<typename T>
	using Tag = internal::Tag<T>;

	/**
	 * @brief Checks if type `T` is an instantiation of template `Template`.
	 * @note This concept works only for templates that have only type template parameters.
	 *       Works for move-only and non-default-constructible `T`
	 */
	template<typename T, template<typename...> typename Template>
	concept IsInstantiationOf = internal::IS_INSTANTIATION_OF_V<T, Template>;

	/**
	 * @brief Checks if type `T` is an instantiation of template `Template`.
	 * @note This concept works only for templates that take one type and one value template
	 * parameter.
	 */
	template<typename T, template<typename, auto> class Template>
	concept IsInstantiationOfTypeValue = internal::IS_INSTANTIATION_OF_TYPE_VALUE_V<T, Template>;

	/**
	 * @brief Checks if type `T` is the same as one of the types in `Types...`
	 */
	template<typename T, typename... Types>
	concept IsOneOf = (std::is_same_v<T, Types> || ...);

	/**
	 * @brief Checks if type `T` is an integral or floating point number but not a character or bool
	 * type
	 */
	template<typename T>
	concept IsNumber
		= std::is_arithmetic_v<T>
	   && not IsOneOf<std::remove_cv_t<T>, bool, char, char8_t, char16_t, char32_t, wchar_t>;

	template<typename T>
	concept IsPlainType = (not std::is_reference_v<T>) and (not std::is_pointer_v<T>);

	/**
	 * This concept is used to statically determine if two types are instances of the same templated
	 * class.
	 *
	 * Example:
	 * @n template<class T>
	 * @n class A { ... };
	 *
	 * template<class T>
	 * @n class B { ... };
	 *
	 * static_assert(IsOfSameClass<A<int>, A<bool>>); // passes
	 * @n static_assert(IsOfSameClass<A<int>, B<int>>);  // fails
	 */
	template<class TypeA, class TypeB>
	concept IsOfSameClass = internal::IsOfSameClassImpl<TypeA, TypeB>::value;

	/**
	 * @brief Type trait to check if a type `T` is a member of a `std::variant`.
	 * @tparam T The type to check.
	 * @tparam VariantT The `std::variant` type.
	 */
	template<typename T, typename VariantT>
	struct is_variant_member: std::false_type {};

	template<typename T, typename... Types>
	struct is_variant_member<T, std::variant<Types...>>:
		  std::disjunction<std::is_same<T, Types>...> {};

	/**
	 * @brief Helper variable template for `is_variant_member`.
	 * @tparam T The type to check.
	 * @tparam VariantT The `std::variant` type.
	 */
	template<typename T, typename VariantT>
	inline constexpr bool IS_VARIANT_MEMBER_V = is_variant_member<T, VariantT>::value;

	/**
	 * @brief Concept that checks if a type `T` is present in a variant `Var`.
	 */
	template<typename T, typename Var>
	concept IsVariantMember = IS_VARIANT_MEMBER_V<T, Var>;

	/**
	 * @brief Concept that checks if a type `T` is a `std::variant`.
	 */
	template<typename T>
	concept IsVariant = IsInstantiationOf<std::remove_cvref_t<T>, std::variant>;

	/**
	 * @brief Type trait to check if a type `T` is present in a tuple `Tup`.
	 * @tparam T The type to check for.
	 * @tparam Tup The tuple type.
	 */
	template<typename T, typename Tup>
	struct is_tuple_member;

	template<typename T, typename... Ts>
	struct is_tuple_member<T, std::tuple<Ts...>>: std::disjunction<std::is_same<T, Ts>...> {};

	/**
	 * @brief Helper variable template for `is_in_tuple`.
	 */
	template<typename T, typename Tup>
	inline constexpr bool IS_TUPLE_MEMBER_V = is_tuple_member<T, Tup>::value;

	/**
	 * @brief Concept that checks if a type `T` is present in a tuple `Tup`.
	 */
	template<typename T, typename Tup>
	concept IsTupleMember = IS_TUPLE_MEMBER_V<T, Tup>;

	/**
	 * @brief A convenient type alias for concatenating multiple tuples.
	 */
	template<typename... Tups>
	using tuple_cat_t = decltype(std::tuple_cat(std::declval<Tups>()...));

	/**
	 * @brief Checks if a variant holds one of the types specified in a tuple.
	 * @tparam TupleOfTypes A std::tuple containing the types to check for.
	 * @param v The variant to check.
	 * @return True if the variant currently holds one of the types from TupleOfTypes, false
	 * otherwise.
	 */
	template<typename TupleOfTypes, typename... VariantArgs>
	constexpr bool variantHoldsOneOf(const std::variant<VariantArgs...>& v) {
		return internal::VariantHoldsOneOfImpl<TupleOfTypes>{}(v);
	}

	/**
	 * @brief Checks if A implies B.
	 */
	template<bool A, bool B>
	concept Implication = !A || B;

	/**
	 * @brief Returns the index of `T` in a `std::variant` at compile time.
	 */
	template<typename VariantT, typename T>
	constexpr usize variantTypeIndex() {
		using ClearedVariantT = std::remove_cvref_t<VariantT>;
		return internal::VariantTypeIndexAux<ClearedVariantT, T>::findIndex();
	}

	/**
	 * @brief Returns the name of the passed type `T`.
	 * @note The type name may not be pretty, and may depend on the compiler and the library
	 * implementation.
	 *
	 * @tparam T The type to get the name of
	 * @note From https://stackoverflow.com/a/56766138
	 */
	template<class T>
	constexpr auto typeName() {
		std::string_view name, prefix, suffix;

#if BASE_TARGET_COMPILER_CLANG
		name   = __PRETTY_FUNCTION__;
		prefix = "auto base::typeName() [T = ";
		suffix = "]";
#elif BASE_TARGET_COMPILER_GCC
		name   = __PRETTY_FUNCTION__;
		prefix = "constexpr auto base::typeName() [with T = ";
		suffix = "]";
#elif BASE_TARGET_COMPILER_MSVC
		name   = __FUNCSIG__;
		prefix = "auto __cdecl base::typeName<";
		suffix = ">(void)";
#endif

		name.remove_prefix(prefix.size());
		name.remove_suffix(suffix.size());
		return name;
	}

	template<typename T>
	class Ref;

	namespace internal {
		template<typename T>
		struct CRefifyParamsImpl;

		template<template<typename...> typename T, typename... Args>
		struct CRefifyParamsImpl<T<Args...>> {
			using type = T<Ref<const Args>...>;
		};
	}

	/**
	 * @brief Wrap template parameters in `CRef`s.
	 * For `T = Foo<Bar1, Bar2, Bar3>`,
	 * `CrefifyParams<T> = Foo<CRef<Bar1>, CRef<Bar2>, CRef<Bar3>>`
	 * This is useful for creating a variant of references from a variant of values.
	 */
	template<typename T>
	using CRefifyParams = internal::CRefifyParamsImpl<T>::type;
}
