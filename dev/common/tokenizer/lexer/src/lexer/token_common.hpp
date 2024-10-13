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
}