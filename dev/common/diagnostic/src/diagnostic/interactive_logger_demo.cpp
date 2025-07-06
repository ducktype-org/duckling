#include "interactive_logger.hpp"
#include <dia_app/view_manager/view_manager.hpp>
#include <dia_app/term_ui/view.hpp>

using namespace dia;

json code_of(std::string code) {
	return json{
		{"type", "code"},
		{"content", code}
	};
}

json entity_of(std::string refers_to, json content) {
	return json{
		{"type", "entity"},
		{"refers_to", refers_to},
		{"content", content}
	};
}

json alt_of(json content, json alt_content) {
	return json{
		{"type", "grouping"},
		{"content", content},
		{"alt_content", alt_content}
	};
}

json meta(std::string type, std::string family, std::string name) {
	return json{
		{"type", type},
		{"family", family},
		{"name", name}
	};
}

json highlight_of(json content, std::string group) {
	return json{
		{"type", "grouping"},
		{"content", content},
		{"groups", json{group}}
	};
}

json start_line(uint i) {
	return json{{"type", "start_line"}, {"number", i}};
}
json start_line() {
	return json{{"type", "start_line"}};
}

json ent_of(std::string kind, json assoc_infos) {
	return json{
		{"kind", kind},
		{"assoc_infos", assoc_infos}
	};
}

json create_demo_from(json j) {
	// ------------------------------------------------------------------------
	// Temporary: create a more interesting sample.
	
	// 1. error
	json &err = j[0];
	
	std::string i32_id = "builtin_type_i32";
	std::string i64_id = "builtin_type_i64";
	std::string plus_id = "builtin_op_plus_i32_i32";
	std::string a_id = "var_a_1010";
	std::string b_id = "var_b_2020";
	std::string x_id = "alias_x_3030";
	
	json i32_el = entity_of(i32_id, code_of("i32"));
	json i64_el = entity_of(i64_id, code_of("i64"));
	json a_el = entity_of(a_id, code_of("a"));
	json b_el = entity_of(b_id, code_of("b"));
	json x_el = entity_of(x_id, code_of("x")); // without alt_content, important for some info's correctness
	json x_el_alt = alt_of(x_el, b_el);
	json plus_el = entity_of(plus_id, code_of("+"));
	json a_plus_a = {
		a_el, code_of(" "), plus_el, code_of(" "), a_el
	};
	err["main_info"]["operator"] = plus_el;
	err["main_info"]["left_type"] = i32_el;
	err["main_info"]["right_type"] = i64_el;
	
	json line_1 = code_of("fun foo() = {");
	json line_2 = {code_of("    "), code_of("let a: "), i32_el, code_of(" = 0;")};
	json line_3 = {code_of("    "), code_of("let b: "), i64_el, code_of(" = 0;")};
	json line_5 = {code_of("    "), code_of("alias x = "), b_el, code_of(";")};
	json expr_raw = {a_el, code_of(" "), plus_el, code_of(" "), a_el, code_of(" + "), x_el};
	json line_7 = {code_of("    return "), highlight_of(expr_raw, "cause"), code_of(";")}; // < with highlight
	json line_8 = code_of("}");
	json code = {
		start_line(1),
		line_1,
		start_line(2),
		line_2,
		start_line(3),
		line_3,
		start_line(4),
		start_line(5),
		line_5,
		start_line(6),
		start_line(7),
		line_7,
		start_line(8),
		line_8
	};
	err["main_info"]["code"]["content"] = code;
	json loc = err["main_info"]["code"]["location"];
	
	json a_decl_loc = loc;
	a_decl_loc["line"] = 2; a_decl_loc["column"] = 5;
	json a_decl_code_content = {
		start_line(2),
		{code_of("    "), highlight_of({code_of("let a: "), i32_el}, "declaration"), code_of(" = 0;")}
	};
	json a_decl_code = {
		{"content", a_decl_code_content},
		{"location", a_decl_loc}
	};
	
	json b_decl_loc = loc;
	b_decl_loc["line"] = 3; b_decl_loc["column"] = 5;
	json b_decl_code_content = {
		start_line(3),
		{code_of("    "), highlight_of({code_of("let b: "), i64_el}, "declaration"), code_of(" = 0;")}
	};
	json b_decl_code = {
		{"content", b_decl_code_content},
		{"location", b_decl_loc}
	};
	
	json x_decl_loc = loc;
	x_decl_loc["line"] = 5; x_decl_loc["column"] = 5;
	json x_decl_code_content = {
		start_line(5),
		{code_of("    "), highlight_of({code_of("alias x = "), b_el}, "declaration"), code_of(";")}
	};
	json x_decl_code = {
		{"content", x_decl_code_content},
		{"location", x_decl_loc}
	};
	
	json first_arg = {
		{"metadata", meta("note", "type_check", "argument_of_type")},
		{"params", json{
			{"argument", a_plus_a},
			{"type", i32_el}
		}}
	};
	json second_arg = {
		{"metadata", meta("note", "type_check", "argument_of_type")},
		{"params", json{
			{"argument", x_el_alt},
			{"type", i64_el}
		}}
	};
	json i32_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i32_el},
			{"bit_count", "32"},
			{"is_signed", "true"}
		}}
	};
	json i64_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i64_el},
			{"bit_count", "64"},
			{"is_signed", "true"}
		}}
	};
	json plus_docs = {
		{"metadata", meta("docs", "builtin", "operator_plus")},
		{"params", {
			{"first_arg_type", i32_el},
			{"second_arg_type", i32_el}
		}}
	};
	json a_decl = {
		{"metadata", meta("note", "declaration", "variable_decl")},
		{"params", {
			{"variable", a_el}
		}},
		{"code", a_decl_code}
	};
	json b_decl = {
		{"metadata", meta("note", "declaration", "variable_decl")},
		{"params", {
			{"variable", b_el}
		}},
		{"code", b_decl_code}
	};
	json x_decl = {
		{"metadata", meta("note", "declaration", "alias_decl")},
		{"params", {
			{"alias", x_el}
		}},
		{"code", x_decl_code}
	};
	json a_type = {
		{"metadata", meta("note", "type_check", "variable_of_type")},
		{"params", {
			{"variable", a_el},
			{"type", i32_el}
		}}
	};
	json b_type = {
		{"metadata", meta("note", "type_check", "variable_of_type")},
		{"params", {
			{"variable", b_el},
			{"type", i64_el}
		}}
	};
	json x_alias = {
		{"metadata", meta("note", "lookup", "name_is_alias")},
		{"params", {
			{"alias_name", x_el},
			{"underlying_name", b_el}
		}}
	};
	err["secondary_infos"] = {
		first_arg,
		second_arg,
		i32_docs,
		i64_docs,
		plus_docs,
		a_decl,
		b_decl,
		x_decl,
		a_type,
		b_type,
		x_alias
	};
	json a_assoc_infos = json::array_t{5, 8};
	json b_assoc_infos = json::array_t{6, 9};
	json x_assoc_infos = json::array_t{7, 10};
	json i32_assoc_infos = json::array_t{2};
	json i64_assoc_infos = json::array_t{3};
	json plus_assoc_infos = json::array_t{4};
	err["displayed_secondary_infos"] = {0, 1};
	
	json a_ent = ent_of("variable", a_assoc_infos);
	json b_ent = ent_of("variable", b_assoc_infos);
	json x_ent = ent_of("alias", x_assoc_infos);
	json i32_ent = ent_of("builtin_type", i32_assoc_infos);
	json i64_ent = ent_of("builtin_type", i64_assoc_infos);
	json plus_ent = ent_of("builtin_operator", plus_assoc_infos);
	
	err["entities"] = {
		{a_id, a_ent},
		{b_id, b_ent},
		{x_id, x_ent},
		{i32_id, i32_ent},
		{i64_id, i64_ent},
		{plus_id, plus_ent}
	};
	// 2. annotations
	
	std::string serialized = j.dump(2);
	j = json::parse(serialized); // deserialize to get uints everywhere
	// ------------------------------------------------------------------------
	return j;
}

json dep_node(
	std::string query, json args, std::string arg_count, json edges, json code={}, json special_arg=""
) {
	json res = {
		{"metadata", meta("note", "features", "dep_tracking_node")},
		{"params", json{
			{"query_name", query},
			{"argument_count", arg_count},
			{"arguments", args},
			{"special_arg", special_arg}
		}},
		{"explore_edges", edges}
	};
	if (!code.empty()) {
		res["code"] = code;
	}
	return res;
}
json graph_node(std::string name, json edges) {
	json res = {
		{"metadata", meta("note", "features", "graph_node")},
		{"params", json{
			{"name", name}
		}},
		{"explore_edges", edges}
	};
	return res;
}
json graph_edge(std::string name, uint handle) {
	return json{
		{"handle", handle},
		{"name", "edge"},
		{"params", json{
			{"edge_name", name}
		}}
	};
}

json demo_loc(uint line, uint col) {
	return json{
		{"file", "dev/compiler/dia_app/view_manager/src/dia_app/view_manager/samples/file2.dmf"},
		{"last_modified", 0},
		{"line", line},
		{"column", col}
	};
}

json edge(uint handle, std::string desc, bool is_main = false) {
	std::string main = is_main ? "true" : "false";
	return json{
		{"handle", handle},
		{"name", "query"},
		{"params", json{
			{"query_name", desc},
			{"is_main", main}
		}}
	};
}

json better_demo() {
	json j;
	j["main_info"] = {
		{"metadata", meta("warning", "features", "why_instanced_feature_on")},
		{"params", json::object_t()}
	};
	// Main function
	std::string A_id = "type_A_1010";
	std::string i64_id = "builtin_type_i64";
	// `A` declaration
	std::string K_id = "type_K_2020";
	std::string i32_id = "builtin_type_i32";
	// `K` declaration
	std::string macro_id = "macro_someMacro_1010";

	json A_el = entity_of(A_id, code_of("A"));
	json i64_el = entity_of(i64_id, code_of("i64"));
	json K_el = entity_of(K_id, code_of("K"));
	json i32_el = entity_of(i32_id, code_of("i32"));
	json macro_el = entity_of(macro_id, code_of("someMacro"));

	json main_code_content = {
		start_line(25),
		code_of("# The function that requires instantiation of A{i64} and therefore K{i32}"),
		start_line(26),
		code_of("fun main() = {"),
		start_line(27),
		{code_of("    "), highlight_of({A_el, code_of("{"), i64_el, code_of("} a")}, "related"), code_of(";")},
		start_line(28),
		code_of("}")
	};
	json main_code = {
		{"content", main_code_content},
		{"location", demo_loc(27, 12)}
	};
	json A_code_content = {
		start_line(18),
		code_of("# Default template parameter is an instance of K"),
		start_line(19),
		{code_of("template{T: type, P = "), K_el, code_of("{"), i32_el, code_of("}: type}")},
		start_line(20),
		{highlight_of(code_of("struct A"), "related"), code_of(" {")},
		start_line(21),
		code_of("    T t;"),
		start_line(22),
		code_of("    P p;"),
		start_line(23),
		code_of("}")
	};
	json A_code = {
		{"content", A_code_content},
		{"location", demo_loc(20, 1)}
	};
	json K_code_content = {
		start_line(12),
		code_of("@why_instanced if (T is "), i32_el, code_of(")"),
		start_line(13),
		highlight_of(code_of("struct K"), "related"), code_of(" {"),
		start_line(14),
		code_of("    @explore"),
		start_line(15),
		{code_of("    "), alt_of(
			{code_of("expand "), macro_el, code_of("(T, t);")},
			{
				code_of("    T t;"),
				start_line(),
				code_of("    fun get() -> T = {"),
				start_line(),
				code_of("        return t;"),
				start_line(),
				code_of("    }")
			}
		)},
		start_line(16),
		code_of("}")
	};
	json K_code = {
		{"content", K_code_content},
		{"location", demo_loc(13, 1)}
	};
	json K_code_content2 = {
		start_line(12),
		code_of("@why_instanced if (T is "), i32_el, code_of(")"),
		start_line(13),
		code_of("struct K"), code_of(" {"),
		start_line(14),
		code_of("    @explore"),
		start_line(15),
		{code_of("    "), alt_of(
			{code_of("expand "), macro_el, code_of("(T, t);")},
			{
				code_of("    T t;"),
				start_line(),
				code_of("    fun get() -> T = {"),
				start_line(),
				code_of("        return t;"),
				start_line(),
				code_of("    }")
			}
		)},
		start_line(16),
		code_of("}")
	};
	json K_code2 = {
		{"content", K_code_content2},
		{"location", demo_loc(13, 1)}
	};
	json macro_code_content = {
		start_line(1),
		{code_of("# Simple macro for struct insides")},
		start_line(2),
		{highlight_of(code_of("macro someMacro(type: Symbol, name: Symbol) {"), "declaration")},
		start_line(3),
		{code_of("    return ${")},
		start_line(4),
		{code_of("        type name;")},
		start_line(5),
		{code_of("        fun get() -> type = {")},
		start_line(6),
		{code_of("            return name;")},
		start_line(7),
		{code_of("        }")},
		start_line(8),
		{code_of("    };")},
		start_line(9),
		{code_of("}")}
	};
	json macro_code = {
		{"content", macro_code_content},
		{"location", demo_loc(2, 1)}
	};

	std::string button_id = "features_button_1010";
	json dep_tracking_req = {
		{"metadata", meta("note", "features", "dep_tracking_requested")},
		{"params", {
			{"button", entity_of(button_id, "[*]")}
		}},
		{"code", K_code2}
	};
	json button_ent = ent_of("features_button", json::array_t{0});
	/*
	Dependency graph (nodes' ids).
		0
		v
		1 > 5
		v
		2 > 6
		v
		3 > 7 > 8
		v
		4
	*/
	json node_0 = dep_node(
		"getType(PST expr) -> Type",
		{"expr=", code_of("a")},
		"one",
		{edge(1, "evalType", true)},
		main_code,
		"expr"
	);
	json node_1 = dep_node(
		"evalType(PST type) -> Type",
		{"type=", A_el, code_of("{"), i64_el, code_of("}")},
		"one",
		{edge(2, "getArgs", true), edge(5, "instantiateType")},
		A_code,
		"type"
	);
	json node_2 = dep_node(
		"getArgs(PST type_name, list<PST> args) -> map<PST, Type>",
		{"type_name=", A_el, ", args={", i64_el, "}"},
		"many",
		{edge(6, "evalType"), edge(3, "evalType", true)}
	);
	json node_3 = dep_node(
		"evalType(PST type) -> Type",
		{"type=", K_el, code_of("{"), i32_el, code_of("}")},
		"one",
		{edge(4, "instantiateType", true), edge(7, "getArgs")},
		K_code,
		"type"
	);
	json node_4 = dep_node(
		"instantiateType(PST type_name, map<PST, Type> args) -> Type",
		{"type_name=", K_el, ", args={T: ", i32_el, "}"},
		"many",
		{}
	);

	json node_5 = dep_node(
		"instantiateType(PST type_name, map<PST, Type> args) -> Type",
		{"type_name=", A_el, ", args={T: ", i64_el, ", P: ", {K_el, code_of("{"), i32_el, code_of("}")}, "}"},
		"many",
		{}
	);
	json node_6 = dep_node(
		"evalType(PST type) -> Type",
		{"type=", i64_el},
		"one",
		{}
	);
	json node_7 = dep_node(
		"getArgs(PST type_name, list<PST> args) -> map<PST, Type>",
		{"type_name=", K_el, ", args={", i32_el, "}"},
		"many",
		{edge(8, "evalType")}
	);
	json node_8 = dep_node(
		"evalType(PST type) -> Type",
		{"type=", i32_el},
		"one",
		{}
	);

	json i32_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i32_el},
			{"bit_count", "32"},
			{"is_signed", "true"}
		}}
	};
	json i64_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i64_el},
			{"bit_count", "64"},
			{"is_signed", "true"}
		}}
	};
	json macro_decl = {
		{"metadata", meta("note", "declaration", "macro_decl")},
		{"params", {
			{"macro", macro_el}
		}},
		{"code", macro_code}
	};

	j["secondary_infos"] = {
		node_0,
		node_1,
		node_2,
		node_3,
		node_4,
		node_5,
		node_6,
		node_7,
		node_8,
		i32_docs,
		i64_docs,
		macro_decl,
		dep_tracking_req
	};
	j["displayed_secondary_infos"] = {
		12
	};
	
	json i32_ent = ent_of("builtin_type", json::array_t{9});
	json i64_ent = ent_of("builtin_type", json::array_t{10});
	json A_ent = ent_of("type", json::array_t{});
	json K_ent = ent_of("type", json::array_t{});
	json macro_ent = ent_of("macro", json::array_t{11});

	j["entities"] = {
		{i32_id, i32_ent},
		{i64_id, i64_ent},
		{A_id, A_ent},
		{K_id, K_ent},
		{macro_id, macro_ent},
		{button_id, button_ent}
	};
	// 2. annotations
	
	std::string serialized = j.dump(2);
	j = json::parse(serialized); // deserialize to get uints everywhere
	j = json::array_t{j};
	
	return j;
}

json error_demo() {
	json j;
	
	std::string A_id = "type_A_1010";
	std::string a_id = "var_a_1010";
	std::string T_id = "type_T_1020";
	std::string t_id = "var_t_1020";
	std::string i64_id = "builtin_type_i64";
	std::string K_id = "type_K_2020";
	std::string i32_id = "builtin_type_i32";
	std::string macro_id = "macro_someMacro_1010";

	json A_el = entity_of(A_id, code_of("A"));
	json a_el = entity_of(a_id, code_of("a"));
	json T_el = entity_of(T_id, code_of("T"));
	json t_el = entity_of(t_id, code_of("t"));
	json i64_el = entity_of(i64_id, code_of("i64"));
	json K_el = entity_of(K_id, code_of("K"));
	json i32_el = entity_of(i32_id, code_of("i32"));
	json macro_el = entity_of(macro_id, code_of("someMacro"));

	json i32_ent = ent_of("builtin_type", json::array_t{0});
	json i64_ent = ent_of("builtin_type", json::array_t{1});
	json A_ent = ent_of("type", json::array_t{3});
	json a_ent = ent_of("variable", json::array_t{3});
	json T_ent = ent_of("type", json::array_t{4});
	json t_ent = ent_of("variable", json::array_t{4});
	json K_ent = ent_of("type", json::array_t{});
	json macro_ent = ent_of("macro", json::array_t{2});

	j["entities"] = {
		{i32_id, i32_ent},
		{i64_id, i64_ent},
		{A_id, A_ent},
		{a_id, a_ent},
		{T_id, T_ent},
		{t_id, t_ent},
		{K_id, K_ent},
		{macro_id, macro_ent}
	};
	
	// for: type A declared here
	json A_code_content = {
		start_line(11),
		{highlight_of(code_of("struct A"), "declaration"), code_of(" {")},
		start_line(12),
		code_of("    p: "), i64_el, code_of(";"),
		start_line(13),
		code_of("    q: "), i64_el, code_of(";"),
		start_line(14),
		code_of("}")
	};
	json A_code = {
		{"content", A_code_content},
		{"location", demo_loc(11, 1)}
	};
	json A_decl = {
		{"metadata", meta("note", "declaration", "type_decl")},
		{"params", {
			{"type", A_el}
		}},
		{"code", A_code}
	};
	// for: type T declared here
	json T_code_content = {
		start_line(16),
		{highlight_of(code_of("struct T"), "declaration"), code_of(" {")},
		start_line(17),
		code_of("    x: "), i32_el, code_of(";"),
		start_line(18),
		code_of("    y: "), i32_el, code_of(";"),
		start_line(19),
		code_of("    z: "), i32_el, code_of(";"),
		start_line(20),
		code_of("}")
	};
	json T_code = {
		{"content", T_code_content},
		{"location", demo_loc(16, 1)}
	};
	json T_decl = {
		{"metadata", meta("note", "declaration", "type_decl")},
		{"params", {
			{"type", T_el}
		}},
		{"code", T_code}
	};
	json K_code_content;
	bool IS_STATIC = true;
	if (IS_STATIC) {
		// for: operator not found (STATIC)
		K_code_content = {
			start_line(22),
			code_of("struct K"), code_of(" {"),
			start_line(23),
			{code_of("    "),
				{
					code_of("t: "), T_el, code_of(";"),
					start_line(24),
					code_of("    fun add(a: "), A_el, code_of(") -> "), T_el, code_of(" = {"),
					start_line(25),
					code_of("        return "), highlight_of({t_el, code_of(" + "), a_el}, "cause"), code_of(";"),
					start_line(26),
					code_of("    }")
				}
			},
			start_line(27),
			code_of("}")
		};
	} else {
		// for: operator not found (INTERACTIVE)
		K_code_content = {
			start_line(22),
			code_of("struct K"), code_of(" {"),
			start_line(23),
			{code_of("    "), alt_of(
				highlight_of({code_of("expand "), macro_el, code_of("(T, t);")}, "cause"),
				{
					code_of("t: "), T_el, code_of(";"),
					start_line(),
					code_of("    fun add(a: "), A_el, code_of(") -> "), T_el, code_of(" = {"),
					start_line(),
					code_of("        return "), highlight_of({t_el, code_of(" + "), a_el}, "cause"), code_of(";"),
					start_line(),
					code_of("    }")
				}
			)
			},
			start_line(24),
			code_of("}")
		};
	}
	json K_code = {
		{"content", K_code_content},
		{"location", demo_loc(22, 1)}
	};
	json macro_code_content = {
		start_line(1),
		{code_of("# Simple macro for struct insides")},
		start_line(2),
		{highlight_of(code_of("macro someMacro(type: Symbol, name: Symbol) {"), "declaration")},
		start_line(3),
		{code_of("    return ${")},
		start_line(4),
		{code_of("        name: type;")},
		start_line(5),
		{code_of("        fun add(a: A) -> type = {")},
		start_line(6),
		{code_of("            return name + a;")},
		start_line(7),
		{code_of("        }")},
		start_line(8),
		{code_of("    };")},
		start_line(9),
		{code_of("}")}
	};
	json macro_code = {
		{"content", macro_code_content},
		{"location", demo_loc(2, 1)}
	};
	json macro_decl = {
		{"metadata", meta("note", "declaration", "macro_decl")},
		{"params", {
			{"macro", macro_el}
		}},
		{"code", macro_code}
	};


	json i32_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i32_el},
			{"bit_count", "32"},
			{"is_signed", "true"}
		}}
	};
	json i64_docs = {
		{"metadata", meta("docs", "builtin", "type_integer")},
		{"params", json{
			{"type", i64_el},
			{"bit_count", "64"},
			{"is_signed", "true"}
		}}
	};

	j["main_info"] = {
		{"metadata", meta("error", "type_check", "no_match_2op")},
		{"params", json{
			{"operator", code_of("+")},
			{"left_type", T_el},
			{"right_type", A_el},
			{"is_static", "false"},
			{"has_expanded_left_type", "false"},
			{"has_expanded_right_type", "false"}
		}},
		{"code", K_code}
	};

	j["secondary_infos"] = {
		i32_docs,
		i64_docs,
		macro_decl,
		A_decl,
		T_decl
	};
	j["displayed_secondary_infos"] = json::array_t{
		
	};
	
	// 2. annotations
	
	std::string serialized = j.dump(2);
	j = json::parse(serialized); // deserialize to get uints everywhere
	j = json::array_t{j};
	
	return j;
}

json graph_demo() {
	json j;
	j["main_info"] = {
		{"metadata", meta("warning", "features", "why_instanced_feature_on")},
		{"params", json::object_t()}
	};
	std::string button_id = "features_button_1010";
	json dep_tracking_req = {
		{"metadata", meta("note", "features", "dep_tracking_requested")},
		{"params", {
			{"button", entity_of(button_id, "[*]")}
		}}
	};
	json button_ent = ent_of("features_button", json::array_t{1});
	/*
	Dependency graph (nodes' ids).
		0
		v
		1 > 5
		v
		2 > 6
		v
		3 > 7 > 8
		v
		4
	*/
	json node_0 = graph_node("A", {graph_edge("B", 2)});
	json node_1 = graph_node("B", {graph_edge("C", 3), graph_edge("F", 6)});
	json node_2 = graph_node("C", {graph_edge("G", 7), graph_edge("D", 4)});
	json node_3 = graph_node("D", {graph_edge("E", 5), graph_edge("H", 8)});
	json node_4 = graph_node("E", {});
	json node_5 = graph_node("F", {});
	json node_6 = graph_node("G", {});
	json node_7 = graph_node("H", {graph_edge("I", 9)});
	json node_8 = graph_node("I", {});

	j["secondary_infos"] = {
		dep_tracking_req,
		node_0,
		node_1,
		node_2,
		node_3,
		node_4,
		node_5,
		node_6,
		node_7,
		node_8
	};
	j["displayed_secondary_infos"] = {
		0
	};
	j["entities"] = {
		{button_id, button_ent}
	};

	std::string serialized = j.dump(2);
	j = json::parse(serialized); // deserialize to get uints everywhere
	j = json::array_t{j};
	
	return j;
}

void dia::InteractiveLogger::m_log(base::Box<InteractiveMessage> message) {
	json j{ message };

	// j = create_demo_from(j);
	// j = better_demo();
	j = error_demo();
	// j = graph_demo();

	// Create a view manager instance for static message.
	auto view_manager = dia_app::view_manager::ViewManager::createFromJson(j);
	::view::ViewResponse *vm_data = new ::view::ViewResponse;
	view_manager.getView(vm_data);

	// Format and print the static message to the terminal.
	term_ui::View term_msg(*vm_data);
	bool use_color = true;
	term_msg.print(std::cerr, use_color);
	delete vm_data;
	
	if (dump_static) {
		// Now we have view data in the format declared in view.proto
		// This is just temporary:
		printer::StreamPrinter::print(j.dump(2), std::cout);
		printer::StreamPrinter::newline(1, std::cout);
		return;
	}
	messages.emplace_back(std::move(message));
}
