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
	 * @brief QResult is a special type used to represent the typical
	 * result of a query or a helper function working within the query framework.
	 * Structurally it behaves similarly to a variant of all provided possible values and implicitly provided special states,
	 * but with certain assumptions used by the query framwork and with interface optimized for implementation of queries.
	 * 
	 * In particular, it can always represent a value of special query::Failed type
	 * which semantically represent opaque failure of a query.
	 * Query framework is aware of this type and can handle/use it in special ways.
	 */
	template<class PrimaryValue, class... SecondaryValues>
	class QResult final {
	private:

		static_assert(
			(!std::is_reference_v<PrimaryValue>) && 
			(... && (!std::is_reference_v<SecondaryValues>)),
			"ErrorValues types should not be references (use CRef instead)"
		);

		static_assert(
			(!std::is_same_v<PrimaryValue, query::Failed>) && 
			(... && (!std::is_same_v<SecondaryValues, query::Failed>)),
			"query::Failed should not be used as an ErrorValue type, it can be represented by default by the QResult"
		);

		/**
		 * Indicates whether the QResult uses a variant to store its value.
		 */
		constexpr static bool USES_VARIANT = sizeof...(SecondaryValues) > 0;

		/** 
		 * Main value type of QResult.
		 */
		using ValueType = std::conditional_t<
			USES_VARIANT,
			std::variant<PrimaryValue, SecondaryValues...>,
			PrimaryValue
		>;

	
	public:
		
		/**
		 * @brief Standard value state constructor.
		 *
		 * @note If variant based value storage is used, provided parameters
		 * are forwarded to the constructor of the variant type.
		 * This means that `std::in_place_type_t<DecidedType>()` can be used as the first parameter
		 * to explicitly specify which type to construct.
		 * This can be especially useful when given parameters can construct
		 * multiple types of the variant and the compiler cannot deduce
		 * which one to use.
		 */
		template<class... Args>
		requires std::is_constructible_v<ValueType, Args...>
		QResult(Args&&... args):
			storage(std::in_place_type_t<ValueType>(), std::forward<Args>(args)...) {}

		/**
		 * @brief Failed state constructor.
		 */
		constexpr QResult(query::Failed) noexcept: storage(query::Failed{}) {}

		constexpr QResult(QResult&&) noexcept            = default;
		constexpr QResult(const QResult&)                = default;
		constexpr QResult& operator=(QResult&&) noexcept = default;
		constexpr QResult& operator=(const QResult&)     = default;

		template<class Value>
		constexpr QResult& operator=(Value&& value) {
			storage = ValueType{std::forward<Value>(value)};
			return *this;
		}

		constexpr QResult& operator=(query::Failed) {
			storage = query::Failed{};
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
		 * @brief Checks if QResult contains one of user specified values.
		 */
		[[nodiscard]]
		constexpr bool hasValue() const {
			return std::holds_alternative<ValueType>(storage);
		}

		/**
		 * @brief Checks if QResult is in Failed state.
		 */
		[[nodiscard]]
		constexpr bool hasFailed() const {
			return std::holds_alternative<query::Failed>(storage);
		}

		// BOOL CAST CAN BE UNINTUITIVE NOW:
		// /**
		//  * @brief Checks if QResult contains a value.
		//  */
		// explicit constexpr operator bool() const { return hasValue(); }



		/**
		 * @brief Access the value as an optional.
		 */
		constexpr base::Optional<base::Ref<ValueType>> optValue() {
			if (hasValue()) return &std::get<ValueType>(storage);
			return {};
		}

		constexpr base::Optional<base::CRef<ValueType>> optValue() const {
			if (hasValue()) return &std::get<ValueType>(storage);
			return {};
		}

		// This was only used in tests:
		// current interface still allows to move from QResult, it simply has to be done explicitly.
		// constexpr base::Optional<ValueType> optValueMove() && {
		// 	if (hasValue()) return std::move(std::get<ValueType>(storage));
		// 	return {};
		// }

		/**
		 * @brief Access the value, panic on no value.
		 */
		constexpr const ValueType& valueOrPanic() const& {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::get<ValueType>(storage);
		}

		constexpr const ValueType&& valueOrPanic() const&& {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::move(std::get<ValueType>(storage));
		}

		constexpr ValueType& valueOrPanic() & {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::get<ValueType>(storage);
		}

		constexpr ValueType&& valueOrPanic() && {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::move(std::get<ValueType>(storage));
		}

		/**
		 * @brief Access the value, throw on no value with a message.
		 * Used when we always except a value to be present.
		 */
		constexpr const ValueType& throwOnFail(std::string_view message) const& {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::get<ValueType>(storage);
		}

		constexpr const ValueType&& throwOnFail(std::string_view message) const&& {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::move(std::get<ValueType>(storage));
		}

		constexpr ValueType& throwOnFail(std::string_view message) & {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::get<ValueType>(storage);
		}

		constexpr ValueType&& throwOnFail(std::string_view message) && {
			if (!hasValue()) throw query::QueryFailedException(message);
			return std::move(std::get<ValueType>(storage));
		}


		/**
		 * @brief Access the value, throw the query failed exception if no value.
		 * This kind of exception can be caught by the query framework.
		 * If you are not handling query exceptions, use valueOrPanic instead.
		 */
		constexpr const ValueType& valueOrThrow() const& {
			if (!hasValue()) throw query::QueryFailedException("Result is empty.");
			return std::get<ValueType>(storage);
		}

		constexpr const ValueType&& valueOrThrow() const&& {
			if (!hasValue()) throw query::QueryFailedException("Result is empty.");
			return std::move(std::get<ValueType>(storage));
		}

		constexpr ValueType& valueOrThrow() & {
			if (!hasValue()) throw query::QueryFailedException("Result is empty.");
			return std::get<ValueType>(storage);
		}

		constexpr ValueType&& valueOrThrow() && {
			if (!hasValue()) throw query::QueryFailedException("Result is empty.");
			return std::move(std::get<ValueType>(storage));
		}

		/** 
		 * @brief Access the value by type, panic on no value.
		 */
		template<typename T>
		constexpr auto getValueByTypeOrPanic() const -> decltype(auto) {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::get<T>(std::get<ValueType>(storage));
		}

		/** 
		 * @brief Test if QResult contains a value of type T.
		 */
		template<typename T>
		[[nodiscard]]
		constexpr bool hasValueByType() const {
			if (!hasValue()) return false;
			return std::holds_alternative<T>(std::get<ValueType>(storage));
		}


	private:

		std::variant<ValueType, query::Failed> storage;
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
#define UNPACK_QRESULT(var, new_value)                                         \
	auto&& RES_VAR_NAME = new_value;                                          \
	if (RES_VAR_NAME.hasFailed()) return query::Failed(); \
	var RES_VAR_NAME.valueOrPanic()

#define UNPACK_QRESULT_MOVE(var, new_value)                                    \
	auto&& RES_VAR_NAME = new_value;                                          \
	if (RES_VAR_NAME.hasFailed()) return query::Failed(); \
	var std::move(RES_VAR_NAME).valueOrPanic()
