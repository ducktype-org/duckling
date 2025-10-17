#pragma once

// Feel free to modify this file, as this code is very generic and tough to write once.

#include <base/except/exceptions.hpp>
#include <base/preproc/utils.hpp>
#include <base/misc/optional.hpp>
#include <base/pointers/ref.hpp>

#include <expected>
#include <type_traits>
#include <utility>
#include <variant>

namespace query {
	namespace impl {
		namespace flatten {
			// Thanks for showing how to unpack and concat variants:
			// https://stackoverflow.com/questions/39272268/creating-a-new-boost-variant-type-from-given-nested-boost-variant-type

			/**
			 * @brief Type of the concatenation of all 'Ts...' tuples.
			 */
			template<typename... Ts>
			using Cat = decltype(std::tuple_cat(std::declval<Ts>()...));

			/**
			 * @brief Flattens a variant of types (including of variants) into a single tuple of
			 * types.
			 */
			template<typename... Ts>
			struct FlattenVariant {
				using type = std::tuple<>;
			};

			/**
			 * @brief Case: T is not a variant.
			 */
			template<class T, class... Ts>
			struct FlattenVariant<T, Ts...> {
				using type = Cat<std::tuple<T>, typename FlattenVariant<Ts...>::type>;
			};

			/**
			 * @brief Case: T is a variant.
			 */
			template<class... VTs, class... Ts>
			struct FlattenVariant<std::variant<VTs...>, Ts...> {
				using type
					= Cat<typename FlattenVariant<VTs...>::type, typename FlattenVariant<Ts...>::type>;
			};

			/**
			 * @brief Convert tuple<Ts...> to variant<Ts...>
			 */
			template<typename T>
			struct ToVariant;

			template<typename... Ts>
			struct ToVariant<std::tuple<Ts...>> {
				using type = std::variant<Ts...>;
			};
		}

		template<typename T>
		using FlattenVariant_t
			= flatten::ToVariant<typename flatten::FlattenVariant<T>::type>::type;

		/**
		 * @brief Check if ToCheck is in [Types...] list of types
		 */
		template<class ToCheck, class... Types>
		struct IsIn: std::bool_constant<(std::is_same_v<ToCheck, Types> || ...)> {};

		template<class... Types>
		concept IsIn_v = IsIn<Types...>::value;

		/**
		 * @brief Transform a list of types into a list of unique types in O(n^2).
		 */
		template<class... Types>
		struct UniqueTypes;

		template<class T>
		struct UniqueTypes<T> {
			using types = std::tuple<T>;
		};

		/**
		 * @brief If First is not in [Second, Rest...], then it is unique
		 */
		template<class First, class Second, class... Rest>
		requires(!IsIn_v<First, Second, Rest...>) struct UniqueTypes<First, Second, Rest...> {
			using types
				= flatten::Cat<std::tuple<First>, typename UniqueTypes<Second, Rest...>::types>;
		};

		/**
		 * @brief If First is in [Second, Rest...], then we don't care about it, and pop it.
		 */
		template<class First, class Second, class... Rest>
		requires(IsIn_v<First, Second, Rest...>) struct UniqueTypes<First, Second, Rest...> {
			using types = UniqueTypes<Second, Rest...>::types;
		};

		/**
		 * @brief Create a variant from a tuple of unique types.
		 */
		template<class... Types>
		struct UniqueTypesToVariant;

		template<class... Types>
		struct UniqueTypesToVariant<std::tuple<Types...>> {
			using types = std::variant<Types...>;
		};
		template<class... Types>
		using UniqueTypesToVariant_t = UniqueTypesToVariant<Types...>::types;

		/**
		 * @brief Create a tuple of unique types from a variant of types.
		 */
		template<class... Types>
		struct VariantToUniqueTypes;

		template<class... Types>
		struct VariantToUniqueTypes<std::variant<Types...>> {
			using types = UniqueTypes<Types...>::types;
		};
		template<class... Types>
		using VariantToUniqueTypes_t = VariantToUniqueTypes<Types...>::types;

		template<class... Types>
		using UniqueTypesVariant_t
			= UniqueTypesToVariant_t<VariantToUniqueTypes_t<FlattenVariant_t<std::variant<Types...>>>>;

		/**
		 * @brief For T and std::variant<Ts...>, check if T is in [Ts...].
		 */
		template<class... Types>
		struct IsInVariant;

		template<class First, class... Rest>
		struct IsInVariant<First, std::variant<Rest...>>: IsIn<First, Rest...> {};

		/**
		 * @brief For std::variant<Ts...>, if Ts... is a single type, then type = Ts, otherwise
		 * type = std::variant<Ts...>.
		 */
		template<class... Types>
		struct SingleVariantExtractor;

		template<class... Types>
		struct SingleVariantExtractor<std::variant<Types...>>: std::true_type {
			using type   = std::variant<Types...>;
			using single = std::true_type;
		};

		template<class T>
		struct SingleVariantExtractor<std::variant<T>>: std::false_type {
			using type   = T;
			using single = std::false_type;
		};

	}  // namespace impl

	template<class... Types>
	using UniqueTypesVariant_t = impl::UniqueTypesVariant_t<Types...>;

	template<class... Types>
	using SingleVariantExtractor = impl::SingleVariantExtractor<Types...>;

	/**
	 * @brief Our implementation of std::unexpected for HELIOS purposes.
	 */
	template<class T>
	struct QError {
		explicit constexpr QError(const T& t): value(t) {}

		T value;
	};

	/**
	 * @brief Our implementation of std::expected for HELIOS purposes.
	 */
	template<class ResTp, class ErrTp1, class... ErrTps>
	requires(!std::is_reference_v<ResTp>) class QResult {
	public:
		using ErrorTypeStruct = SingleVariantExtractor<UniqueTypesVariant_t<ErrTp1, ErrTps...>>;
		using ErrorIsVariant  = std::is_base_of<std::true_type, ErrorTypeStruct>;
		/**
		 * @brief The most important using in this class. If multiple error types, holds a variant
		 * of unique types, otherwise error_type is equal to the provided type. Error list is
		 * variant-transparent.
		 *
		 * QResult<T, int>				-> error_type == int
		 * QResult<T, int, float> 		-> error_type == std::variant<int, float>
		 * QResult<T, int, float, std::variant<int, bool>>
		 * 		-> error_type == std::variant<float, int, bool>
		 *
		 * In the last example, notice unique types, and also a modified order (implementation
		 * reasons...)
		 */
		using ErrorType = ErrorTypeStruct::type;

		/**
		 * @brief Constructor from QError<T>, where T is not a variant
		 */
		template<class T>
		requires(not ErrorIsVariant::value or impl::IsInVariant<T, ErrorType>::value)
		constexpr QResult(const QError<T>& err): storage(std::unexpected(err.value)) {}

		/**
		 * @brief Constructor from QError<T>, where T is a variant
		 */
		template<class T>
		constexpr QResult(const QError<T>& err) {
			std::visit([&](auto&& err_value) { storage = std::unexpected(err_value); }, err.value);
		}

		constexpr QResult(QResult&&) noexcept            = default;
		constexpr QResult(const QResult&)                = default;
		constexpr QResult& operator=(QResult&&) noexcept = default;
		constexpr QResult& operator=(const QResult&)     = default;

		constexpr QResult& operator=(const ResTp& value) {
			storage = value;
			return *this;
		}

		constexpr QResult& operator=(ResTp&& value) {
			storage = std::move(value);
			return *this;
		}

		/**
		 * @brief Copy constructor from QResult, where Ts... are a subset of this QResult error
		 * types
		 */
		template<class T, class... Ts>
		requires std::is_constructible_v<ResTp, T> constexpr QResult(const QResult<T, Ts...>& oth) {
			// Cannot use the initializer list, because oth.value_storage is private (different
			// types)
			if (oth.hasValue()) storage.emplace(oth.value());

			if (oth.hasError()) {
				if constexpr (QResult<T, Ts...>::ErrorIsVariant::value)
					std::visit(
						[&](auto&& er_tp) { storage = std::unexpected(ErrorType{ er_tp }); },
						oth.error()
					);
				else
					storage = std::unexpected(oth.error());
			}
		}

		template<class... Args>
		requires std::is_constructible_v<ResTp, Args...>
		QResult(Args&&... args): storage(std::in_place, std::forward<Args>(args)...) {}

		/**
		 * @brief Checks if QResult contains an error.
		 */
		[[nodiscard]]
		constexpr bool hasError() const {
			return !storage.has_value();
		}

		/**
		 * @brief Checks if QResult contains a value.
		 */
		[[nodiscard]]
		constexpr bool hasValue() const {
			return storage.has_value();
		}

		/**
		 * @brief Checks if QResult contains a value.
		 */
		explicit constexpr operator bool() const { return hasValue(); }

		/**
		 * @brief Access the value, throw on no value.
		 */
		constexpr const ResTp& value() const& { return expect("Result is empty!"); }

		constexpr const ResTp&& value() const&& { return std::move(expect("Result is empty!")); }

		constexpr ResTp& value() & { return expect("Result is empty!"); }

		constexpr ResTp&& value() && { return std::move(expect("Result is empty!")); }

		/**
		 * @brief Access the value as an optional.
		 */
		constexpr base::Optional<base::Ref<ResTp>> optValue() {
			if (storage.has_value()) return &storage.value();
			return {};
		}

		constexpr base::Optional<base::CRef<ResTp>> optValue() const {
			if (storage.has_value()) return &storage.value();
			return {};
		}

		constexpr base::Optional<ResTp> optValueMove() && {
			if (storage.has_value()) return std::move(storage.value());
			return {};
		}

		/**
		 * @brief Access the value, throw on no value with a message.
		 */
		constexpr const ResTp& expect(std::string_view message) const& {
			if (!storage.has_value()) throw std::runtime_error(std::string(message));
			return storage.value();
		}

		constexpr const ResTp&& expect(std::string_view message) const&& {
			if (!storage.has_value()) throw std::runtime_error(std::string(message));
			return std::move(storage.value());
		}

		constexpr ResTp& expect(std::string_view message) & {
			if (!storage.has_value()) throw std::runtime_error(std::string(message));
			return storage.value();
		}

		constexpr ResTp&& expect(std::string_view message) && {
			if (!storage.has_value()) throw std::runtime_error(std::string(message));
			return std::move(storage.value());
		}

		/**
		 * @brief Access the error, throw on no error.
		 */
		constexpr const ErrorType& error() const& { return storage.error(); }

		constexpr const ErrorType&& error() const&& { return std::move(storage.error()); }

		constexpr ErrorType& error() & { return storage.error(); }

		constexpr ErrorType&& error() && { return storage.error(); }

		/**
		 * @brief Access the value, throw the error if no value.
		 */
		constexpr const ResTp& valueOrThrow() const& {
			if (hasError()) throw error();
			return storage.value();
		}

		constexpr const ResTp&& valueOrThrow() const&& {
			if (hasError()) throw error();
			return std::move(storage.value());
		}

		constexpr ResTp& valueOrThrow() & {
			if (hasError()) throw error();
			return storage.value();
		}

		constexpr ResTp&& valueOrThrow() && {
			if (hasError()) throw error();
			return std::move(storage.value());
		}


	private:
		std::expected<ResTp, ErrorType> storage;
	};
}

/**
 * @brief This is a unique variable per macro - assuming every macro is in a separate line.
 */
#define RES_VAR_NAME CONCAT_2(result_storage_aBz4vq2_, __LINE__)

/**
 * @brief Since C++ doesn't have an error-propagating operator, this macro
 * essentially implements it - checks if `value` has an error and if it does, then
 * returns an error as well, otherwise stores an unpacked value
 * inside a new variable named `name`.
 * For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
 * **ATTENTION** This macro is not a single instruction, so it means if you have an if-statement
 * before it, you need to put the call inside curly braces. Luckily, it will NOT COMPILE otherwise.
 */
#define UNPACK_RESULT(var, new_value)                                         \
	auto&& RES_VAR_NAME = new_value;                                          \
	if (!RES_VAR_NAME.hasValue()) return query::QError(RES_VAR_NAME.error()); \
	var RES_VAR_NAME.value()

#define UNPACK_RESULT_MOVE(var, new_value)                                    \
	auto&& RES_VAR_NAME = new_value;                                          \
	if (!RES_VAR_NAME.hasValue()) return query::QError(RES_VAR_NAME.error()); \
	var std::move(RES_VAR_NAME).value()
