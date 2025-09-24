#include "operator_not_found.hpp"

using nlohmann::json;

json dia::OperatorNotFound::Params::tojson() {
	json lhs_type, rhs_type;
	// TODO: make this better, eg. creating class for entities
	json code       = json::object();
	code["type"]    = "grouping";
	code["content"] = lhs->expression_type.getType().toString();
	lhs_type["refers_to"]
		= std::to_string(lhs->expression_type.getType().queryUnstablePerfectHash());
	lhs_type["content"] = code;
	lhs_type["type"]    = "entity";
	code["content"]     = rhs->expression_type.getType().toString();
	rhs_type["refers_to"]
		= std::to_string(rhs->expression_type.getType().queryUnstablePerfectHash());
	rhs_type["content"] = code;
	rhs_type["type"]    = "entity";
	return json{ { "operator", op.strView() },
		         { "left_type", lhs_type },
		         { "right_type", rhs_type },
		         { "is_static", "false" },
		         { "has_expanded_left_type", "false" },
		         { "has_expanded_right_type", "false" } };
}

using compiler::helios::code::Expr;
