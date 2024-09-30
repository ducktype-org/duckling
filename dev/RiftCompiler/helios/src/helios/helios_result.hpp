#pragma once

#include <base/exceptions.hpp>
#include <base/define_helper.hpp>
#include <type_traits>
#include <variant>
#include <base/optional.hpp>

namespace compiler::helios::errors {
	namespace impl {
		namespace flatten {
			// Thanks for showing how to unpack and concat variants:
			// https://stackoverflow.com/questions/39272268/creating-a-new-boost-variant-type-from-given-nested-boost-variant-type

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

		// Check if ToCheck is in [Types...] list of types
		template<class ToCheck, class... Types>
		struct is_in: std::bool_constant<(std::is_same_v<ToCheck, Types> || ...)> {};

		template<class... Types>
		inline constexpr bool is_in_v = is_in<Types...>::value;

		// Transform a list of types into a list of unique types in O(n^2).
		template<class... Types>
		struct unique_types;

		template<class T>
		struct unique_types<T> {
			using types = std::tuple<T>;
		};

		// If First is not in [Second, Rest...], then it is unique
		template<class First, class Second, class... Rest>
		requires(!is_in_v<First, Second, Rest...>) struct unique_types<First, Second, Rest...> {
			using types
				= flatten::cat<std::tuple<First>, typename unique_types<Second, Rest...>::types>;
		};

		// If First is in [Second, Rest...], then we don't care about it, and pop it.
		template<class First, class Second, class... Rest>
		requires(is_in_v<First, Second, Rest...>) struct unique_types<First, Second, Rest...> {
			using types = unique_types<Second, Rest...>::types;
		};

		// Create a variant from a tuple of unique types.
		template<class... Types>
		struct unique_types_to_variant;

		template<class... Types>
		struct unique_types_to_variant<std::tuple<Types...>> {
			using types = std::variant<Types...>;
		};
		template<class... Types>
		using unique_types_to_variant_t = unique_types_to_variant<Types...>::types;

		// Create a tuple of unique types from a variant of types.
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

		// For T and std::variant<Ts...>, check if T is in [Ts...].
		template<class... Types>
		struct is_in_variant;

		template<class First, class... Rest>
		struct is_in_variant<First, std::variant<Rest...>>: is_in<First, Rest...> {};

		// For std::variant<Ts...>, if Ts... is a single type, then type = Ts, otherwise
		// type = std::variant<Ts...>.
		template<class... Types>
		struct single_variant_extractor;

		template<class... Types>
		struct single_variant_extractor<std::variant<Types...>>: std::true_type {
			using type = std::variant<Types...>;
		};

		template<class T>
		struct single_variant_extractor<std::variant<T>>: std::false_type {
			using type = T;
		};

	}  // namespace impl

	template<class... Types>
	using unique_types_variant_t = impl::unique_types_variant_t<Types...>;

	template<class... Types>
	using single_variant_extractor = impl::single_variant_extractor<Types...>;

	template<class T>
	struct HError {
		explicit constexpr HError(const T& t): value(t) {}

		T value;
	};

	template<class ResTp, class ErrTp1, class... ErrTps>
	requires std::is_trivially_copyable_v<ErrTp1> && (std::is_trivially_copyable_v<ErrTps> && ...)
	class HResult {
	public:
		using error_type_struct
			= single_variant_extractor<unique_types_variant_t<ErrTp1, ErrTps...>>;
		using error_is_variant = std::is_base_of<std::true_type, error_type_struct>;
		/**
		 * @brief The most important using in this class. If multiple error types, holds a variant
		 * of unique types, otherwise error_type is equal to the provided type. Error list is
		 * variant-transparent.
		 *
		 *  HResult<T, int>									-> error_type == int
		 *  HResult<T, int, float> 							-> error_type == std::variant<int,
		 * float> HResult<T, int, float, std::variant<int, bool>> -> error_type ==
		 * std::variant<float, int, bool> In the last example, notice unique types, and also a
		 * modified order (implementation reasons...)
		 */
		using error_type = error_type_struct::type;

		/**
		 * @brief Constructor from HError<T>, where T is not a variant
		 */
		template<class T>
		requires(not error_is_variant::value or impl::is_in_variant<T, error_type>::value)
		constexpr HResult(const HError<T>& err): error_storage(err.value) {}

		/**
		 * @brief Constructor from HError<T>, where T is a variant
		 */
		template<class T>
		constexpr HResult(const HError<T>& err) {
			std::visit([&](auto&& err_value) { error_storage = err_value; }, err.value);
		}

		constexpr HResult(HResult&&) noexcept            = default;
		constexpr HResult(const HResult&)                = default;
		constexpr HResult& operator=(HResult&&) noexcept = default;
		constexpr HResult& operator=(const HResult&)     = default;

		/**
		 * @brief Copy constructor from HResult, where Ts... are a subset of this HResult error
		 * types
		 */
		template<class... Ts>
		constexpr HResult(const HResult<ResTp, Ts...>& oth) {
			// Cannot use the initializer list, because oth.value_storage is private (different
			// types)
			if (oth.has_value()) value_storage = oth.value();
			if (oth.has_error())
				std::visit([&](auto&& erTp) { error_storage = error_type{ erTp }; }, oth.error());
		}

		/**
		 * @brief Constructor of the main value by forwarding arguments
		 */
		template<class... Args>
		requires std::is_constructible_v<base::Optional<ResTp>, Args...>
		constexpr HResult(Args&&... args): value_storage(std::forward<Args>(args)...) {}

		/**
		 * @brief Checks if HResult contains an error.
		 */
		[[nodiscard]]
		constexpr bool has_error() const {
			return !value_storage.has_value();
		}

		/**
		 * @brief Checks if HResult contains a value.
		 */
		[[nodiscard]]
		constexpr bool has_value() const {
			return !has_error();
		}

		/**
		 * @brief Access the value, throw on no value.
		 */
		constexpr const ResTp& value() const& { return expect("Result it empty!"); }

		constexpr const ResTp&& value() const&& { return std::move(expect("Result it empty!")); }

		constexpr ResTp& value() & { return expect("Result it empty!"); }

		constexpr ResTp&& value() && { return std::move(expect("Result it empty!")); }

		/**
		 * @brief Access the value, throw on no value with a message.
		 */
		constexpr const ResTp& expect(std::string_view message) const& {
			_throwOnInvalidStateAccess();
			return value_storage.expect(message);
		}

		constexpr const ResTp&& expect(std::string_view message) const&& {
			_throwOnInvalidStateAccess();
			return std::move(value_storage.expect(message));
		}

		constexpr ResTp& expect(std::string_view message) & {
			_throwOnInvalidStateAccess();
			return value_storage.expect(message);
		}

		constexpr ResTp&& expect(std::string_view message) && {
			_throwOnInvalidStateAccess();
			return std::move(value_storage.expect(message));
		}

		/**
		 * @brief Access the error, throw on no error.
		 */
		constexpr const error_type& error() const& {
			_throwOnInvalidStateAccess();
			return error_storage.expect("Result's error is empty!");
		}

		constexpr const error_type&& error() const&& {
			_throwOnInvalidStateAccess();
			return std::move(error_storage.expect("Result's error is empty!"));
		}

		constexpr error_type& error() & {
			_throwOnInvalidStateAccess();
			return error_storage.expect("Result's error is empty!");
		}

		constexpr error_type&& error() && {
			_throwOnInvalidStateAccess();
			return std::move(error_storage.expect("Result's error is empty!"));
		}

		/**
		 * @brief Access the value, throw the error if no value.
		 */
		constexpr const ResTp& valueOrThrow() const& {
			_throwOnInvalidStateAccess();
			if (has_error()) throw error();
			return value_storage.value();
		}

		constexpr const ResTp&& valueOrThrow() const&& {
			_throwOnInvalidStateAccess();
			if (has_error()) throw error();
			return std::move(value_storage.value());
		}

		constexpr ResTp& ValueOrThrow() & {
			_throwOnInvalidStateAccess();
			if (has_error()) throw error();
			return value_storage.value();
		}

		constexpr ResTp&& valueOrThrow() && {
			_throwOnInvalidStateAccess();
			if (has_error()) throw error();
			return std::move(value_storage.value());
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
// **ATTENTION** This macro is not a single instruction, so it means if you have an if-statement
// before it, you need to put the call inside curly braces. Luckily, it will NOT COMPILE otherwise.
#define UNPACK_RESULT(var, new_value)                                                             \
	auto&& RES_VAR_NAME = new_value;                                                              \
	if (!RES_VAR_NAME.has_value()) return compiler::helios::errors::HError(RES_VAR_NAME.error()); \
	var RES_VAR_NAME.value()
