#pragma once

#include <base/define_helper.hpp>
#include <variant>
#include <base/optional.hpp>
#include <iostream>

namespace compiler::helios::errors {
	// Thanks for showing how to unpack and concat variants:
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


		template<class... Types>
		struct is_in_variant;

		template<class First, class... Others>
		struct is_in_variant<First, std::variant<Others...>>: is_in<First, Others...> {};
	}  // namespace impl

	template<class... Types>
	using unique_types_variant_t = impl::unique_types_variant_t<Types...>;

	template<class T>
	struct HUnexpected {
		explicit constexpr HUnexpected(const T& t): value(t) {}

		T value;
	};

	template<class ResTp, class ErrTp1, class... ErrTps>
	requires std::is_trivially_copyable_v<ErrTp1> && (std::is_trivially_copyable_v<ErrTps> && ...)
	class HResult {
	public:
		using error_type = unique_types_variant_t<ErrTp1, ErrTps...>;

		// Constructor from Unexpected<T>, where T is not a variant
		template<class T>
		requires impl::is_in_variant<T, error_type>::value
		constexpr HResult(const HUnexpected<T>& err) {
			error_storage = err.value;
		}

		// Constructor from Unexpected<T>, where T is a variant
		template<class T>
		constexpr HResult(const HUnexpected<T>& err) {
			std::visit([&](auto&& err_value) { error_storage = err_value; }, err.value);
		}

		// Move constructor
		constexpr HResult(HResult&&) noexcept = default;
		// Copy constructor
		constexpr HResult(const HResult&) = default;
		// Move = operator (potentially we will need more to support all the constructors as well?)
		constexpr HResult& operator=(HResult&&) noexcept = default;
		// Copy = operator
		constexpr HResult& operator=(const HResult&) = default;

		// Copy constructor from HResult, where Ts... are a subset of this HResult types
		template<class... Ts>
		constexpr HResult(const HResult<ResTp, Ts...>& oth) {
			// Cannot use the initializer list, because oth.value_storage is private (different
			// types)
			if (oth.has_value()) value_storage = oth.value();
			if (oth.has_error())
				std::visit([&](auto&& erTp) { error_storage = error_type{ erTp }; }, oth.error());
		}

		// Move constructor from HResult, where Ts... are a subset of this HResult types
		template<class... Ts>
		constexpr HResult(HResult<ResTp, Ts...>&& oth) {
			// Cannot use the initializer list, because oth.value_storage is private (different
			// types)
			if (oth.has_value()) value_storage = std::move(oth.value());
			if (oth.has_error())
				std::visit(
					[&](auto&& erTp) { error_storage = error_type{ erTp }; }, std::move(oth.error())
				);
		}

		// Constructor of the main value by forwarding arguments
		template<class... Args>
		requires std::is_constructible_v<base::Optional<ResTp>, Args...>
		constexpr HResult(Args&&... args): value_storage(std::forward<Args>(args)...) {}

		[[nodiscard]]
		constexpr bool has_error() const {
			return !value_storage.has_value();
		}

		[[nodiscard]]
		constexpr bool has_value() const {
			return !has_error();
		}

		constexpr const ResTp& value() const& {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_value(), "Result is empty!");
			return value_storage.value();
		}

		constexpr const ResTp&& value() const&& {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_value(), "Result is empty!");
			return std::move(value_storage.value());
		}

		constexpr ResTp& value() & {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_value(), "Result is empty!");
			return value_storage.value();
		}

		constexpr ResTp&& value() && {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_value(), "Result is empty!");
			return std::move(value_storage.value());
		}

		constexpr const error_type& error() const& {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_error(), "Error is empty!");
			return error_storage.value();
		}

		constexpr const error_type&& error() const&& {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_error(), "Error is empty!");
			return std::move(error_storage.value());
		}

		constexpr error_type& error() & {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_error(), "Error is empty!");
			return error_storage.value();
		}

		constexpr error_type&& error() && {
			_throwOnInvalidStateAccess();
			RIFT_ASSERT(has_error(), "Error is empty!");
			return std::move(error_storage.value());
		}

	private:
		base::Optional<error_type> error_storage;
		base::Optional<ResTp>      value_storage;

		void _throwOnInvalidStateAccess() const {
			RIFT_ASSERT(
				error_storage.has_value() ^ value_storage.has_value(),
				"HResult has an invalid state"
			);
		}
	};
}

// This is a unique variable per macro - assuming every macro is in a separate line.
#define RES_VAR_NAME CONCAT_2(result_storage_aBz4vq2_, __LINE__)

// Since C++ doesn't have an error-propagating operator, this macro
// essentially implements it - checks if `value` has an error and if it does, then
// returns an error as well, otherwise stores an unpacked value
// inside a new variable named `name`.
// For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
#define UNPACK_RESULT(new_value, name)      UNPACK_RESULT_CUSTOM(new_value, auto&& name)
#define UNPACK_RESULT_COPY(new_value, name) UNPACK_RESULT_CUSTOM(new_value, auto name)

#define UNPACK_RESULT_CUSTOM(new_value, name)                               \
	auto&& RES_VAR_NAME = new_value;                                        \
	if (!RES_VAR_NAME.has_value())                                          \
		return compiler::helios::errors::HUnexpected(RES_VAR_NAME.error()); \
	name = RES_VAR_NAME.value()

#define UNPACK_OR_PANIC(result, message)       \
	[](auto&& res) {                           \
		RIFT_ASSERT(res.has_value(), message); \
		return res.value();                    \
	}(result)
