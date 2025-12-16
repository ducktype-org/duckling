#pragma once

// Feel free to modify this file, as this code is very generic and tough to write once.

#include "query_errors.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>
#include <base/preproc/utils.hpp>

#include <expected>
#include <type_traits>
#include <variant>

namespace query {
	
	
	/**
	 * @brief Analogy of std::unexpected for QResult.
	 */
	template<class T>
	struct QError final {
		T value;
	};


	/**
	 * @brief QResult is a special type used to represent the typical result of a query or a helper function working within the query framework.
	 * Structurally it behaves similarly to
	 * std::variant<MainValue, std::variant<ErrorValues...>, query::Failed>, but with some differences:
	 * 
	 * It can always represent a value of special query::Failed type
	 * which semantically represent opaque failure of a query.
	 * Query framework is aware of this type and can handle/use it specially.
	 */
	template<class MainValue, class... ErrorValues>
	requires(!std::is_reference_v<MainValue>) class QResult final {
	private:
		/**
		 * This type is only used to fill the variant when there are no ErrorValues.
		 */
		struct DummyType final {};

	    /**
		 * @note This could be made a simple type when sizeof...(ErrorValues) == 0 or == 1,
		 * but for uniformity of the code we always use the variant-based implementation.
		 */
		using ErrorVariantType = std::variant<ErrorValues..., DummyType>;

		using ResultType = MainValue;

		struct MainResultHolder final {
			MainValue value;
		};

		struct ErrorResultHolder final {
			ErrorVariantType error;
		};


	public:

		
		/**
		 * @brief MainValue state constructor.
		 */
		template<class... Args>
		requires std::is_constructible_v<MainValue, Args...>
		QResult(Args&&... args):
			storage(std::in_place_type_t<MainResultHolder>(), std::forward<Args>(args)...) {}


		/**
		 * @brief Error state constructor.
		 * Constructor from QError<T>, where T is not a variant
		 */
		template<class T>
		requires(std::is_constructible_v<ErrorVariantType, T>)
		constexpr QResult(const QError<T>& err):
			storage(std::in_place_type_t<ErrorResultHolder>(), err.value) {}

		/**
		 * @brief Constructor from QError<T>, where T is a variant
		 */
		template<class... T>
		constexpr QResult(const QError<std::variant<T...>>& err) {
			std::visit([&](auto&& err_value) { storage = ErrorResultHolder{err_value}; }, err.value);
		}

		/**
		 * @brief Failed state constructor.
		 */
		constexpr QResult(query::Failed) noexcept: storage(query::Failed{}) {}

		constexpr QResult(QResult&&) noexcept            = default;
		constexpr QResult(const QResult&)                = default;
		constexpr QResult& operator=(QResult&&) noexcept = default;
		constexpr QResult& operator=(const QResult&)     = default;

		constexpr QResult& operator=(const MainValue& value) {
			storage = MainResultHolder{value};
			return *this;
		}

		constexpr QResult& operator=(MainValue&& value) {
			storage = MainResultHolder{std::move(value)};
			return *this;
		}

		// /**
		//  * @brief Copy constructor from QResult, where Ts... are a subset of this QResult error
		//  * types
		//  */
		// template<class T, class... Ts>
		// requires std::is_constructible_v<MainValue, T> constexpr QResult(const QResult<T, Ts...>& oth) {
		// 	// Cannot use the initializer list, because oth.value_storage is private (different
		// 	// types)
		// 	if (oth.hasValue()) storage = oth.valueOrThrow();

		// 	if (oth.hasError()) {
		// 		if constexpr (QResult<T, Ts...>::ErrorIsVariant::value)
		// 			std::visit(
		// 				[&](auto&& er_tp) { storage = std::unexpected(ErrorType{ er_tp }); },
		// 				oth.error()
		// 			);
		// 		else
		// 			storage = std::unexpected(oth.error());
		// 	}
		// }


		/**
		 * @brief Checks if QResult contains a value.
		 */
		[[nodiscard]]
		constexpr bool hasValue() const {
			return std::holds_alternative<MainResultHolder>(storage);
		}

		/**
		 * @brief Checks if QResult contains an error.
		 */
		[[nodiscard]]
		constexpr bool hasError() const {
			return std::holds_alternative<ErrorResultHolder>(storage);
		}

		/**
		 * @brief Checks if QResult is in Failed state.
		 */
		[[nodiscard]]
		constexpr bool isFailed() const {
			return std::holds_alternative<query::Failed>(storage);
		}


		/**
		 * @brief Checks if QResult contains a value.
		 */
		explicit constexpr operator bool() const { return hasValue(); }

		/**
		 * @brief Access the value as an optional.
		 */
		constexpr base::Optional<base::Ref<MainValue>> optValue() {
			if (hasValue()) return &std::get<MainResultHolder>(storage).value;
			return {};
		}

		constexpr base::Optional<base::CRef<MainValue>> optValue() const {
			if (hasValue()) return &std::get<MainResultHolder>(storage).value;
			return {};
		}

		constexpr base::Optional<MainValue> optValueMove() && {
			if (hasValue()) return std::move(std::get<MainResultHolder>(storage).value);
			return {};
		}

		/**
		 * @brief Access the value, panic on no value.
		 */
		constexpr const MainValue& valueOrPanic() const& {
			if (hasError()) CORE_PANIC("Result is empty.");
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr const MainValue&& valueOrPanic() const&& {
			if (hasError()) CORE_PANIC("Result is empty.");
			return std::move(std::get<MainResultHolder>(storage).value);
		}

		constexpr MainValue& valueOrPanic() & {
			if (hasError()) CORE_PANIC("Result is empty.");
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr MainValue&& valueOrPanic() && {
			if (hasError()) CORE_PANIC("Result is empty.");
			return std::move(std::get<MainResultHolder>(storage).value);
		}

		/**
		 * @brief Access the value, throw on no value with a message.
		 * Used when we always except a value to be present.
		 */
		constexpr const MainValue& throwOnFail(std::string_view message) const& {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr const MainValue&& throwOnFail(std::string_view message) const&& {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::move(std::get<MainResultHolder>(storage).value);
		}

		constexpr MainValue& throwOnFail(std::string_view message) & {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr MainValue&& throwOnFail(std::string_view message) && {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::move(std::get<MainResultHolder>(storage).value);
		}

		/**
		 * @brief Access the error, throw on no error.
		 */
		constexpr const ErrorVariantType& error() const& { return std::get<ErrorResultHolder>(storage).error; }

		constexpr const ErrorVariantType&& error() const&& { return std::move(std::get<ErrorResultHolder>(storage).error); }

		constexpr ErrorVariantType& error() & { return std::get<ErrorResultHolder>(storage).error; }

		constexpr ErrorVariantType&& error() && { return std::move(std::get<ErrorResultHolder>(storage).error); }

		template<typename T>
		constexpr auto getErrorByType() const -> decltype(auto) {
			return std::get<T>(std::get<ErrorResultHolder>(storage).error);
		}

		/**
		 * @brief Access the value, throw the query failed exception if no value.
		 * This kind of exception can be caught by the query framework.
		 * If you are not handling query exceptions, use valueOrPanic instead.
		 */
		constexpr const MainValue& valueOrThrow() const& {
			if (hasError()) throw query::QueryFailedException("Result is empty.");
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr const MainValue&& valueOrThrow() const&& {
			if (hasError()) throw query::QueryFailedException("Result is empty.");
			return std::move(std::get<MainResultHolder>(storage).value);
		}

		constexpr MainValue& valueOrThrow() & {
			if (hasError()) throw query::QueryFailedException("Result is empty.");
			return std::get<MainResultHolder>(storage).value;
		}

		constexpr MainValue&& valueOrThrow() && {
			if (hasError()) throw query::QueryFailedException("Result is empty.");
			return std::move(std::get<MainResultHolder>(storage).value);
		}


	private:

		std::variant<MainResultHolder, ErrorResultHolder, query::Failed> storage;
	};

	template<typename T>
	struct IsQResult: std::false_type {};

	template<typename... Args>
	struct IsQResult<query::QResult<Args...>>: std::true_type {};
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
	var RES_VAR_NAME.valueOrThrow()

#define UNPACK_RESULT_MOVE(var, new_value)                                    \
	auto&& RES_VAR_NAME = new_value;                                          \
	if (!RES_VAR_NAME.hasValue()) return query::QError(RES_VAR_NAME.error()); \
	var std::move(RES_VAR_NAME).valueOrThrow()
