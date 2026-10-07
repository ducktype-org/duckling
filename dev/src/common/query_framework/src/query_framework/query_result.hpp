// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

// Feel free to modify this file, as this code is very generic and tough to write once.

#include "query_errors.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <type_traits>
#include <variant>

namespace query {

	/**
	 * @brief QResult is a special type used to represent the typical
	 * result of a query or a helper function working within the query framework.
	 *
	 * Structurally it behaves similarly to a variant of a @tp Value type and special states
	 * provided implicitly by the query framwork (currently query::Failed state).
	 * Query framework is aware of such special states and can handle/use them in special ways.
	 * Crucially valueOrThrow method throws a special query::QueryFailedException, that can be
	 * automatically caught by the query framework to mark the query as failed.
	 */
	template<class Value>
	class QResult final {
	private:
		static_assert(
			!std::is_reference_v<Value>, "Value type should not be references (use CRef instead)"
		);

		static_assert(
			(!std::is_same_v<Value, query::Failed>),
			"query::Failed should not be used as an Value type, it can be represented by "
			"default by the QResult"
		);

	public:
		/**
		 * @brief Standard value state constructor.
		 */
		template<class... Args>
		requires std::is_constructible_v<Value, Args...> QResult(Args&&... args):
			  storage(std::in_place_type_t<Value>(), std::forward<Args>(args)...) {}

		/**
		 * @brief Failed state constructor.
		 */
		constexpr QResult(query::Failed) noexcept: storage(query::Failed{}) {}

		constexpr QResult(QResult&&) noexcept            = default;
		constexpr QResult(const QResult&)                = default;
		constexpr QResult& operator=(QResult&&) noexcept = default;
		constexpr QResult& operator=(const QResult&)     = default;

		/**
		 * @brief Assignment forming a Value state.
		 */
		template<class OthValue>
		requires std::is_constructible_v<Value, OthValue&&>
		constexpr QResult& operator=(OthValue&& value) {
			storage = Value{ std::forward<OthValue>(value) };
			return *this;
		}

		/**
		 * @brief Assignment forming a Failed state.
		 */
		constexpr QResult& operator=(query::Failed) {
			storage = query::Failed{};
			return *this;
		}

		/**
		 * @brief Checks if QResult contains one of user specified values.
		 */
		[[nodiscard]]
		constexpr bool hasValue() const {
			return std::holds_alternative<Value>(storage);
		}

		/**
		 * @brief Checks if QResult is in Failed state.
		 */
		[[nodiscard]]
		constexpr bool hasFailed() const {
			return std::holds_alternative<query::Failed>(storage);
		}

		/**
		 * @brief Access the value as an optional.
		 */
		constexpr base::Optional<base::Ref<Value>> optValue() {
			if (hasValue()) return &std::get<Value>(storage);
			return {};
		}

		constexpr base::Optional<base::CRef<Value>> optValue() const {
			if (hasValue()) return &std::get<Value>(storage);
			return {};
		}

		/**
		 * @brief Access the value, panic on no value.
		 */
		constexpr const Value& valueOrPanic() const& {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::get<Value>(storage);
		}

		constexpr const Value&& valueOrPanic() const&& {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::move(std::get<Value>(storage));
		}

		constexpr Value& valueOrPanic() & {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::get<Value>(storage);
		}

		constexpr Value&& valueOrPanic() && {
			if (!hasValue()) CORE_PANIC("Result is empty.");
			return std::move(std::get<Value>(storage));
		}

		/**
		 * @brief Access the value, panic with given message on no value.
		 */
		constexpr const Value& valueOrPanicMsg([[maybe_unused]] std::string_view message) const& {
			if (!hasValue()) CORE_PANIC("Result is empty: {}", message);
			return std::get<Value>(storage);
		}

		constexpr const Value&& valueOrPanicMsg([[maybe_unused]] std::string_view message) const&& {
			if (!hasValue()) CORE_PANIC("Result is empty: {}", message);
			return std::move(std::get<Value>(storage));
		}

		constexpr Value& valueOrPanicMsg([[maybe_unused]] std::string_view message) & {
			if (!hasValue()) CORE_PANIC("Result is empty: {}", message);
			return std::get<Value>(storage);
		}

		constexpr Value&& valueOrPanicMsg([[maybe_unused]] std::string_view message) && {
			if (!hasValue()) CORE_PANIC("Result is empty: {}", message);
			return std::move(std::get<Value>(storage));
		}

		/**
		 * @brief Access the value, throw the query failed exception if no value.
		 * This kind of exception can be caught by the query framework.
		 * If you are not handling query exceptions, use valueOrPanic instead.
		 */
		constexpr const Value& valueOrThrow() const& {
			if (!hasValue()) throwFailed("Result is empty.");
			return std::get<Value>(storage);
		}

		constexpr const Value&& valueOrThrow() const&& {
			if (!hasValue()) throwFailed("Result is empty.");
			return std::move(std::get<Value>(storage));
		}

		constexpr Value& valueOrThrow() & {
			if (!hasValue()) throwFailed("Result is empty.");
			return std::get<Value>(storage);
		}

		constexpr Value&& valueOrThrow() && {
			if (!hasValue()) throwFailed("Result is empty.");
			return std::move(std::get<Value>(storage));
		}


	private:
		std::variant<Value, query::Failed> storage;
	};

	template<typename T>
	struct IsQResult: std::false_type {};

	template<typename... Args>
	struct IsQResult<query::QResult<Args...>>: std::true_type {};
}

/**
 * @brief This is a unique variable per macro - assuming every macro is in a separate line.
 */
#define RES_VAR_NAME CAT(result_storage_aBz4vq2_, __LINE__)

/**
 * @brief Since C++ doesn't have an error-propagating operator, this macro
 * essentially implements it - checks if `value` is in Failed state and if it does, then
 * returns a Failed state as well, otherwise stores an unpacked value
 * inside a new variable named `name`.
 * For interested: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2561r1.html#ref-P2561R0
 * **ATTENTION** This macro is not a single instruction, so it means if you have an if-statement
 * before it, you need to put the call inside curly braces. Luckily, it will NOT COMPILE otherwise.
 */
#define UNPACK_QRESULT(var, new_value)                    \
	auto&& RES_VAR_NAME = new_value;                      \
	if (RES_VAR_NAME.hasFailed()) return query::Failed(); \
	var RES_VAR_NAME.valueOrPanic()

#define UNPACK_QRESULT_MOVE(var, new_value)               \
	auto&& RES_VAR_NAME = new_value;                      \
	if (RES_VAR_NAME.hasFailed()) return query::Failed(); \
	var std::move(RES_VAR_NAME).valueOrPanic()

/**
 * Unpack a result from QResult or return the Failed state.
 * In situations where you have a CRef<QResult<...>>
 */
#define UNPACK_QRESULT_CREF(var, new_value)                \
	auto&& RES_VAR_NAME = new_value;                       \
	if (RES_VAR_NAME->hasFailed()) return query::Failed(); \
	var RES_VAR_NAME->valueOrPanic()

/**
 * Unpack a result from QResult or return the Failed state.
 * Used in situations where you have a CRef<QResult<Box<...>>>,
 */
#define UNPACK_QRESULT_CREF_TO_BOX(var, new_value)         \
	auto&& RES_VAR_NAME = new_value;                       \
	if (RES_VAR_NAME->hasFailed()) return query::Failed(); \
	var RES_VAR_NAME->valueOrPanic().ref()


/**
 * @brief Helper macros mirroring the optional helpers, but for QResult.
 *
 * Example:
 * ```cpp
 * auto getNumber() -> query::QResult<int>;
 * match_qresult(getNumber()) {
 *     qres_value(val) { std::cout << "value: " << val << '\n'; }
 *     qres_failed      { std::cout << "query failed" << '\n'; }
 * }
 *
 * if_qres_value(getNumber(), num) {
 *     std::cout << "num squared: " << num * num;
 * }
 * ```
 */
#define match_qresult(qresult)                                          \
	PUSH_DIAGNOSTIC                                                     \
	NO_SHADOW                                                           \
	if (bool _qres_perform_match = true)                                \
		for (auto&& _internal_qresult = (qresult); _qres_perform_match; \
		     _qres_perform_match      = false)                          \
	POP_DIAGNOSTIC

#define qres_value(_value_name)                                                       \
	PUSH_DIAGNOSTIC                                                                   \
	NO_SHADOW                                                                         \
	if (bool _qres_perform_if = _internal_qresult.hasValue())                         \
		for (auto&& _value_name = _internal_qresult.valueOrPanic(); _qres_perform_if; \
		     _qres_perform_if   = false)                                              \
	POP_DIAGNOSTIC

#define qres_value_move(_value_name)                                                             \
	PUSH_DIAGNOSTIC                                                                              \
	NO_SHADOW                                                                                    \
	if (bool _qres_perform_if = _internal_qresult.hasValue())                                    \
		for (auto&& _value_name = std::move(_internal_qresult).valueOrPanic(); _qres_perform_if; \
		     _qres_perform_if   = false)                                                         \
	POP_DIAGNOSTIC

#define qres_failed                                              \
	PUSH_DIAGNOSTIC                                              \
	NO_SHADOW                                                    \
	if (bool _qres_failed_guard = _internal_qresult.hasFailed()) \
		for (; _qres_failed_guard; _qres_failed_guard = false) POP_DIAGNOSTIC

#define if_qres_value(qresult, _value_name)                                                   \
	PUSH_DIAGNOSTIC                                                                           \
	NO_SHADOW                                                                                 \
	if (auto&& _internal_qresult = (qresult); _internal_qresult.hasValue())                   \
		if (bool _if_qres_value_stop = true)                                                  \
			for (auto&& _value_name  = _internal_qresult.valueOrPanic(); _if_qres_value_stop; \
			     _if_qres_value_stop = false)                                                 \
	POP_DIAGNOSTIC

#define if_qres_failed(qresult)                                                                    \
	PUSH_DIAGNOSTIC                                                                                \
	NO_SHADOW                                                                                      \
	if (auto&& _internal_qresult = (qresult); _internal_qresult.hasFailed())                       \
		for (bool _if_qres_failed_stop = true; _if_qres_failed_stop; _if_qres_failed_stop = false) \
	POP_DIAGNOSTIC
