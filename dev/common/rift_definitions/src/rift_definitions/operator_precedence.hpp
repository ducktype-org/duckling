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

#include <base/string_id.hpp>
#include "key_spec_op.hpp"

namespace rift_def {

	namespace operator_precedence {
		void init();
	}

	enum class OperatorType {
		Binary, UnaryLeft, UnaryRight, Nullary 
	};

	enum class OperatorAssociativity {
		LeftToRight, RightToLeft 
	};

	i64 operatorPrecedence(base::StrId operator_, OperatorType operator_type);
	i64 operatorPrecedence(Operator operator_, OperatorType operator_type);

	OperatorAssociativity operatorAssociativity(base::StrId operator_, OperatorType operator_type);
	OperatorAssociativity operatorAssociativity(Operator operator_, OperatorType operator_type);

}

