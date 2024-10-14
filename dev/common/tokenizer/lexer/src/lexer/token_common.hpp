#pragma once

#include <base/string_id.hpp>
#include <rift_definitions/key_spec_op.hpp>

namespace lexer {
	/**
	 * @brief Simple wrapper for an operator
	 */
	struct Operator final {
		const base::StrID value;

		Operator() = delete;
		Operator(const base::StrID id): value(id) {};
		Operator(const rift_def::NamedOperator op): value(rift_def::operatorToStr(op)) {};
		Operator(const Operator&) = default;

		operator base::StrID() { return value; }

		std::string str() const {
			return value.str();
		}

		rift_def::NamedOperator asNamed() const {
			return rift_def::strAsOperator(value);
		}

		/**
		 * @note This should do the corrected UTF-8 check in the future.
		 */
		inline bool operator==(rift_def::NamedOperator& op) { return rift_def::operatorToStr(op) == value; }

		inline bool operator==(Operator& other) { return *this == other.value; }
	};

	/**
	 * @brief Simple wrapper for a value
	 */
	struct Value final {
		const base::StrID value;

		Value();
		Value(const base::StrID id): value(id) {};
		Value(const std::string str): value(base::StrID(str.c_str())) {};
		Value(const Value&) = default;

		operator base::StrID() { return value; }

		std::string str() const {
			return value.str();
		}

		/**
		 * @note This should probably do something more in the future
		 */
		inline bool operator==(Value& other) { return value == other.value; }
	};
}