/**
 * @author Andrzej
 *
 * Only following operators are supported for now for testing purposes:
 *
 * In order or precedence:
 *
 * `a.b` -- Left To Right
 * `++a`, `-a` -- Right To Left
 * `a*b`, 'a/b' -- Left To Right
 * `a+b`, 'a-b' -- Left To Right
 * `a=b` -- Right To Left
 *
 */

#pragma once


#include "key_spec_op.hpp"

#include <init/init.hpp>

namespace lang_def {

	namespace operator_precedence {
		/**
		 * Initializes the module.
		 * Will be called automagically when InitObject is used.
		 */
		void init();

		RUN_BEFORE_MAIN(init::registerForInit(operator_precedence::init));
	}

	enum class OperatorType { Binary, UnaryLeft, UnaryRight, Nullary };

	enum class OperatorAssociativity { LeftToRight, RightToLeft };

	i64 operatorPrecedence(base::StrID operatorr, OperatorType operator_type);
	i64 operatorPrecedence(NamedOperator operatorr, OperatorType operator_type);

	OperatorAssociativity operatorAssociativity(base::StrID operatorr, OperatorType operator_type);
	OperatorAssociativity operatorAssociativity(NamedOperator operatorr, OperatorType operator_type);
}
