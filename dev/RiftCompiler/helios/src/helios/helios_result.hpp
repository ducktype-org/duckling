#pragma once

#include <base/define_helper.hpp>
#include <variant>
#include <base/optional.hpp>

namespace compiler::helios::errors {
	// Thanks to:
	// https://stackoverflow.com/questions/39272268/creating-a-new-boost-variant-type-from-given-nested-boost-variant-type
	namespace impl {
		namespace flatten {
			// Type of the concatenation of all 'Ts...' tuples.
			template<typename... Ts>
			using cat = decltype(std::tuple_cat(std::declval<Ts>()...));

			template<typename TResult, typename... Ts>
			struct flatten_variant;

			// Base case: no more types to process.
			template<typename TResult>
			struct flatten_variant<TResult> {
				using type = TResult;
			};

			// Case: T is not a variant.
			// Return concatenation of previously processed types,
			// T, and the flattened remaining types.
			template<typename TResult, typename T, typename... TOther>
			struct flatten_variant<TResult, T, TOther...> {
				using type = cat<
					TResult,
					std::tuple<T>,
					typename flatten_variant<TResult, TOther...>::type>;
			};

			// Case: T is a variant.
			// Return concatenation of previously processed types,
			// the types inside the variant, and the flattened remaining types.
			// The types inside the variant are recursively flattened in a new
			// flatten_variant instantiation.
			template<typename TResult, typename... Ts, typename... TOther>
			struct flatten_variant<TResult, std::variant<Ts...>, TOther...> {
				using type = cat<
					TResult,
					typename flatten_variant<std::tuple<>, Ts...>::type,
					typename flatten_variant<TResult, TOther...>::type>;
			};

			// Forward decl
			template<typename T>
			struct to_variant;

			// Convert tuple<Ts...> to variant<Ts...>
			template<typename... Ts>
			struct to_variant<std::tuple<Ts...>> {
				using type = std::variant<Ts...>;
			};
		}

		template<typename T>
		using flatten_variant_t
			= flatten::to_variant<typename flatten::flatten_variant<std::tuple<>, T>::type>::type;

		// Check if ToCheck is in [FirstElement, Rest...] list of types
		template<class ToCheck, class FirstElement, class... Rest>
		struct is_in;

		template<class ToCheck, class FirstElement, class... Rest>
		requires(!std::is_same_v<ToCheck, FirstElement>)
		struct is_in<ToCheck, FirstElement, Rest...>: is_in<ToCheck, Rest...> {};

		template<class ToCheck, class FirstElement, class... Rest>
		requires(std::is_same_v<ToCheck, FirstElement>)
		struct is_in<ToCheck, FirstElement, Rest...>: std::integral_constant<bool, true> {};

		template<class ToCheck, class Tail>
		struct is_in<ToCheck, Tail>: std::is_same<ToCheck, Tail> {};

		template<class... Tps>
		inline constexpr bool is_in_v = is_in<Tps...>::value;

		// Transform a list of types into a list of unique types in O(n^2).
		template<class... Types>
		struct unique_types;

		template<class Tp>
		struct unique_types<Tp> {
			using types = std::tuple<Tp>;
		};

		template<class First, class Second, class... Rest>
		requires(!is_in_v<First, Second, Rest...>) struct unique_types<First, Second, Rest...> {
			using types
				= flatten::cat<std::tuple<First>, typename unique_types<Second, Rest...>::types>;
		};

		template<class First, class Second, class... Rest>
		requires(is_in_v<First, Second, Rest...>) struct unique_types<First, Second, Rest...> {
			using types = unique_types<Second, Rest...>::types;
		};

		template<class... Types>
		struct unique_types_to_variant;

		template<class... Types>
		struct unique_types_to_variant<std::tuple<Types...>> {
			using types = std::variant<Types...>;
		};
		template<class... Types>
		using unique_types_to_variant_t = unique_types_to_variant<Types...>::types;

		template<class... Types>
		struct variant_to_unique_types;

		template<class... Types>
		struct variant_to_unique_types<std::variant<Types...>> {
			using types = unique_types<Types...>::types;
		};
		template<class... Types>
		using variant_to_unique_types_t = variant_to_unique_types<Types...>::types;

		template<class... Types>
		using unique_types_variant = unique_types_to_variant_t<
			variant_to_unique_types_t<flatten_variant_t<std::variant<Types...>>>>;
	}  // namespace impl

	template<class... Types>
	using unique_types_variant_t = impl::unique_types_variant<Types...>;

	template<class ResTp, class ErrTp1, class... ErrTps>
	requires std::is_trivially_copyable_v<ResTp> && std::is_trivially_copyable_v<ErrTp1>
	      && (std::is_trivially_copyable_v<ErrTps> && ...) class HResult {
	public:
		using error_type = unique_types_variant_t<ErrTp1, ErrTps...>;

		template<class... Args>
		requires std::is_constructible_v<ResTp, Args...>
		HResult(Args&&... args): result(std::forward<Args>(args)...) {}

		template<class... Args>
		requires std::is_constructible_v<error_type, Args...>
		HResult(Args&&... args): error_value(std::forward<Args>(args)...) {}

		[[nodiscard]]
		bool has_error() const {
			RIFT_ASSERT(
				error_value.valueless_by_exception() ^ !result.has_value(),
				"HResult has an invalid state"
			);
			return !result.has_value();
		}

		[[nodiscard]]
		bool has_value() const {
			return !has_error();
		}

		ResTp value() {
			RIFT_ASSERT(result.has_value(), "Result is empty!");
			return result.value();
		}

		error_type error() {
			RIFT_ASSERT(error_value.valueless_by_exception(), "Error is empty!");
			return error_value.value();
		}

	private:
		base::Optional<ResTp> result;
		error_type            error_value;
	};
}
