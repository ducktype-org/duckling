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
		template<class ToCheck, class... Types>
		struct is_in: std::bool_constant<(std::is_same_v<ToCheck, Types> || ...)> {};

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
		using unique_types_variant_t = unique_types_to_variant_t<
			variant_to_unique_types_t<flatten_variant_t<std::variant<Types...>>>>;
	}  // namespace impl

	template<class... Types>
	using unique_types_variant_t = impl::unique_types_variant_t<Types...>;

	template<class T>
	struct HUnexpected {
		template<class... Args>
		explicit HUnexpected(T&& t): value(t) {}

		T value;
	};

	template<class ResTp, class ErrTp1, class... ErrTps>
	requires std::is_trivially_copyable_v<ErrTp1> && (std::is_trivially_copyable_v<ErrTps> && ...)
	class HResult {
	public:
		using error_type = unique_types_variant_t<ErrTp1, ErrTps...>;

		template<class... Args>
		requires std::is_constructible_v<ResTp, Args...>
		HResult(Args&&... args): result(std::forward<Args>(args)...) {}

		template<class T>
		HResult(const HUnexpected<T>& err): error_value(err.value) {}

		template<class... Ts>
		HResult(const HResult<Ts...>::error_type& err) {
			std::visit([&](auto&& erTp) { error_value = error_type{ erTp }; }, err);
		}

		template<class... Ts>
		HResult(const HResult<ResTp, Ts...>& oth) {
			if (oth.has_value()) result = oth.result;
			if (oth.has_error())
				std::visit([&](auto&& erTp) { error_value = error_type{ erTp }; }, oth.error_value);
		}

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

		ResTp value() const {
			RIFT_ASSERT(has_value(), "Result is empty!");
			return result.value();
		}

		error_type error() const {
			RIFT_ASSERT(has_error(), "Error is empty!");
			return error_value;
		}

	private:
		base::Optional<ResTp> result;
		error_type            error_value;
	};
}
