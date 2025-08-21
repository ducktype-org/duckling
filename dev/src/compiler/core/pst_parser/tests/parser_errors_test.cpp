#include "pst_parser/elements/hierarchy/not_statements/patterns.hpp"

#include <pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <pst_parser/pst.hpp>
#include <pst_parser/pst_visitor.hpp>

#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

#include <sstream>
#include <utility>

class PSTErrorTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PSTErrorTests

	struct GenExample;
	static std::vector<GenExample*> examples;

	struct GenExample {
		std::string code;

		GenExample(std::string code): code(std::move(code)) { examples.push_back(this); }

		virtual bool operator()() = 0;
		[[nodiscard]]
		virtual std::string message() const
			= 0;

		virtual ~GenExample() = default;
	};

	template<typename Element, bool good = true, typename Parser = Element>
	struct Example: public GenExample {
		Example(std::string code): GenExample(std::move(code)) {}

		bool operator()() override {
			auto parsed = pst::PST<Element, Parser>::fromContents(code);
			// parsed.getLogger()->dumpLog(true);
			return parsed.getLogger()->good() == good;
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << code << "` as ";
			ss << base::typeName<Element>();
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<std::derived_from<pst::ClassStmt> Element, bool good = true, typename Parser = Element>
	struct ClassStmtExample: public GenExample {
		pst::ClassContext context;

		ClassStmtExample(std::string code, pst::ClassContext&& ctx):
			  GenExample(std::move(code)),
			  context(std::move(ctx)) {}

		ClassStmtExample(std::string code):
			  GenExample(std::move(code)),
			  context{ base::StrID("unnamed"), {} } {}

		ClassStmtExample(std::string code, const std::string& class_name):
			  GenExample(std::move(code)),
			  context{ base::StrID(class_name.c_str()), {} } {}

		bool operator()() override {
			auto parsed = pst::PST<Element, Parser>::fromContentsWithContext(this->code, context);
			return parsed.getLogger()->good() == good;
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << this->code << "` as ";
			ss << base::typeName<Element>();
			ss << " in class `" << context.name.str() << "`";
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<typename Element, bool good>
	void testExample(Example<Element, good>& example) {
		assertTrue(example(), example.message());
	}

	Example<pst::Alias, true>  simple_alias{ "alias sqrt=std.math.sqrt" };
	Example<pst::Alias, false> alias_star{ "alias math=std.math.*" };

	Example<pst::Attribute, true> simple_attr{ "@pretty(5)" };

	Example<pst::Block, true>  simple_block{ "block {}" };
	Example<pst::Block, false> no_block{ "block;" };
	Example<pst::Block, false> no_block_eof{ "block" };

	Example<pst::CodeBlockOrStmt, true> just_block{ "{}" };
	Example<pst::CodeBlockOrStmt, true> just_stmt{ "x=y;" };

	Example<pst::CodeBlock, true>  simple_code_block{ "{}" };
	Example<pst::CodeBlock, false> no_code_block{ "x=y;" };
	Example<pst::CodeBlock, false> no_code_block_eof{ "" };

	Example<pst::Const, true>  simple_const{ "const x: i32 = 5" };
	Example<pst::Const, true>  ref_const{ "const x: ref i32 = 5" };
	Example<pst::Const, true>  type_const{ "const x: i32" };
	Example<pst::Const, true>  value_const{ "const x = 5" };
	Example<pst::Const, false> no_name_const{ "const: i32 = 5" };
	Example<pst::Const, false> no_type_const{ "const x:= 5" };
	Example<pst::Const, false> no_value_const{ "const x: i32=;" };
	Example<pst::Const, false> no_value_const_eof{ "const x: i32=" };
	Example<pst::Const, false> let_const{ "let x: i32 = 5" };
	Example<pst::Const, false> var_const{ "var x: i32 = 5" };

	Example<pst::Variable, true>  simple_var{ "var x: i32 = 5" };
	Example<pst::Variable, true>  let_var{ "let x: i32 = 5" };
	Example<pst::Variable, true>  ref_var{ "var x: ref i32 = 5" };
	Example<pst::Variable, true>  type_var{ "var x: i32" };
	Example<pst::Variable, true>  value_var{ "var x = 5" };
	Example<pst::Variable, false> no_name_var{ "var: i32 = 5" };
	Example<pst::Variable, false> no_type_var{ "var x:= 5" };
	Example<pst::Variable, false> no_value_var{ "var x: i32=;" };
	Example<pst::Variable, false> no_value_var_eof{ "var x: i32=" };
	Example<pst::Variable, false> const_var{ "const x: i32 = 5" };

	Example<pst::DottedName, true>  simple_dotted{ "std.a.b.*;" };
	Example<pst::DottedName, false> bad_dotted{ "std.a.b. .*" };

	Example<pst::Const, false> bad_stmt_choice{ "block {}" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  simple_expr{ "x + y" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  block_expr{ "x + {return 2 * x;}" };
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad_token_expr{ "\"" };

	Example<pst::Fun, true>  simple_function1{ "fun foo(x: i32, y: i32) -> (i32, i32) = {}" };
	Example<pst::Fun, true>  simple_function2{ "fun foo(x: i32, y: i32 = 1) = {}" };
	Example<pst::Fun, false> bad_function{ "fun foo(x: i32, y) = {}" };

	Example<pst::Pattern, true> simple_pattern1{ "pattern IsEven(x: i32) = {}" };
	Example<pst::Pattern, true> simple_pattern2{
		"pattern Point(p: Point) -> (i32, i32) = {return (p.x, p.y);}"
	};
	Example<pst::Pattern, false> no_params_pattern{ "pattern Point() = {}" };
	Example<pst::Pattern, false> two_params_pattern{ "pattern Point(a: T, b: T) = {}" };
	Example<pst::Pattern, false> trailing_comma_pattern{ "pattern Point(a: T,) = {}" };

	Example<pst::If, true> simple_if{ "if (a == b) {c = d;}" };
	Example<pst::If, true> simple_if_else{ "if (a == b) {c = d;} else {c = e;}" };
	Example<pst::If, true> simple_if_else_no_blocks{ "if (a == b) c = d; else c = e;" };

	Example<pst::Import, true> simple_import{ "import std.math.sqrt as sqrt" };

	Example<pst::Namespace, true> simple_namespace{ "namespace name {}" };

	Example<pst::RoundGroupExpr, true>  simple_round_group{ "(a + b)" };
	Example<pst::RoundGroupExpr, false> bad_round_group{ "a + b" };

	Example<pst::Stmt, true>  simple_stmt{ "x = a + b;" };
	Example<pst::Stmt, true>  expand_stmt{ "expand \"return 0;\";" };
	Example<pst::Stmt, false> bad_stmt{ "x = a + b" };

	Example<pst::TopLevel, true> simple_top_level{ "fun foo() = {}" };

	Example<pst::Using, true> simple_using{ "using std.math" };

	Example<pst::StmtSpecifier, true>  public_specifier{ "public expand \"return 0;\";" };
	Example<pst::StmtSpecifier, true>  private_specifier{ "private fun foo() = {}" };
	Example<pst::StmtSpecifier, true>  protected_specifier{ "protected class x{}" };
	Example<pst::StmtSpecifier, true>  public_block{ "public {class x{}}" };
	Example<pst::StmtSpecifier, false> bad_specifier{ "def class x{}" };
	Example<pst::StmtSpecifier, false> empty_specifier{ "public" };

	Example<pst::For, true>  simple_for{ "for(a in a.b(x, y)) {}" };
	Example<pst::For, true>  simple_typed_for{ "for(a: T, U in a + c) {}" };
	Example<pst::For, false> empty_type_for{ "for(a: in a + c) {}" };
	Example<pst::For, false> no_in_for{ "for(a a + c) {}" };

	Example<pst::Class, true>  simple_class{ "class x{}" };
	Example<pst::Class, true>  complicated_class{ "class x extends y implements z:{}, d:{T} {}" };
	Example<pst::Class, false> empty_extends_class{ "class x extends {}" };
	Example<pst::Class, false> empty_extends_class2{ "class x extends implements z {}" };
	Example<pst::Class, false> multiple_extends_class{ "class x extends y, z {}" };

	Example<pst::Expand, true>  nested_expand{ R"(expand "expand \"return 0;\";")" };
	Example<pst::Expand, false> empty_expand{ "expand ;" };
	Example<pst::Expand, false> unclosed_expand{ "expand \"return 0;;" };

	ClassStmtExample<pst::NonClassStmt, true> class_using{ "using std.math;" };
	ClassStmtExample<pst::NonClassStmt, true> class_alias{ "alias sqrt=std.math.sqrt;" };

	ClassStmtExample<pst::AccessBlock, true>  public_access_block{ "public {}" };
	ClassStmtExample<pst::AccessBlock, true>  private_access_block{ "private {}" };
	ClassStmtExample<pst::AccessBlock, true>  protected_access_block{ "protected {}" };
	ClassStmtExample<pst::AccessBlock, false> multi_specifier_block{ "public private {}" };

	ClassStmtExample<pst::Field, true>  simple_field{ "x: i32 = 5" };
	ClassStmtExample<pst::Field, true>  simple_specified_field{ "public static x: i32 = 5" };
	ClassStmtExample<pst::Field, false> bad_field{ "x = 5" };
	ClassStmtExample<pst::Field, false> bad_field2{ "x : = 5" };

	ClassStmtExample<pst::Method, true> simple_method{
		"fun foo(x: i32, y: i32) -> (i32, i32) = {}"
	};

	ClassStmtExample<pst::Constructor, true> default_constructor{ "name(x: i32) = {}", "name" };
	ClassStmtExample<pst::Constructor, true> named_constructor{
		"name.from_pair(p: (i32, i32)) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, true> init_constructor{
		"name.init(x: i32, y: i32): z(x, y) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> bad_constructor1{
		"name.(x: i32, y: i32): z(x, y) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> bad_constructor2{ "name.(x: i32, y: i32) -> i32 = {}",
		                                                        "name" };

	ClassStmtExample<pst::Destructor, true>  simple_destructor{ "name.destroy() = {}", "name" };
	ClassStmtExample<pst::Destructor, false> non_empty_destructor{ "name.destroy(x: i32) = {}",
		                                                           "name" };

	// @todo Some weird position bug for later
	// Example<pst::Variable, true> simpleVariable{"var x: i32 = 5"};
	// Example<pst::Variable, true> simpleLetVariable{"let x: i32 = 5"};

	Example<pst::While, true> simple_while{ "while (x < 5) {}" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_ternary{
		"if 5 then '\\n' else y"
	};
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad1_ternary{
		"if if 5 then x else y"
	};
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad2_ternary{
		"+ if 5 then x else y"
	};
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad3_ternary{ "if 5 else y" };

	Example<pst::ExprStmt, true>  simple_assign{ "x = y" };
	Example<pst::ExprStmt, true>  simple_string_assign{ "x = \"left\"" };
	Example<pst::ExprStmt, false> bad_assign{ "x = y = z" };
	Example<pst::ExprStmt, false> bad_operator{ "x = y z + 3" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_operators{ "++ ++ 3 + 5 ++" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder> text_operator{ "++ ++ 3 + 5 kg ++" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder> new_operators{ "<> 3 <> 'x' <>" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder> all_integer_operators{
		"1 + 2 - 3 * 4 / 5 % 6 ** 7"
	};
	Example<pst::ExprHolder, true, pst::UniversalExprHolder> all_boolean_operators{
		"true and true or false and not false"
	};
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  prefix_named{ "ref const T" };
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad_operators{ "++ ++ ++ ++" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_block_expr{ "x + {return 2;}" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_round_expr{ "x + (x, y)" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_chain_expr{
		"(x * t).y.z(4)[3]"
	};

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_template_expr{
		"(x * t).y:{x, y}.z:{}(4)[3]"
	};

	Example<pst::FlowPattern, true>  flow_literal_int{ "123" };
	Example<pst::FlowPattern, true>  flow_literal_string{ "\"hello world\"" };
	Example<pst::FlowPattern, true>  flow_literal_bool{ "true" };
	Example<pst::FlowPattern, true>  flow_binding_simple{ "my_variable" };
	Example<pst::FlowPattern, true>  flow_wildcard{ "_" };
	Example<pst::FlowPattern, true>  flow_tuple_simple{ "(1, x)" };
	Example<pst::FlowPattern, true>  flow_tuple_nested{ "(1, (x, _))" };
	Example<pst::FlowPattern, false> flow_tuple_empty{ "()" };
	Example<pst::FlowPattern, false> flow_tuple_unclosed{ "(1, x" };
	Example<pst::FlowPattern, false> flow_tuple_trailing_comma{ "(1, x,)" };
	Example<pst::FlowPattern, true>  flow_deconstructor_simple{ "Point(x, y)" };
	Example<pst::FlowPattern, true>  flow_deconstructor_nested{ "Circle(Point(0, 0), r)" };
	Example<pst::FlowPattern, false> flow_deconstructor_empty_args{ "None()" };
	Example<pst::FlowPattern, false> flow_deconstructor_unclosed{ "Point(x, y" };
	Example<pst::FlowPattern, true>  flow_just_literal{ "1" };
	Example<pst::FlowPattern, true>  flow_just_binding{ "x" };
	Example<pst::FlowPattern, true>  flow_just_tuple{ "(a, b)" };
	Example<pst::FlowPattern, true>  flow_just_deconstructor{ "Some(v)" };
	Example<pst::FlowPattern, true>  flow_as_binding_simple{ "x as my_var" };
	Example<pst::FlowPattern, true>  flow_as_binding_tuple{ "(a, b) as point" };
	Example<pst::FlowPattern, true>  flow_as_binding_deconstructor{ "Rectangle(w, h) as rect" };
	Example<pst::FlowPattern, false> flow_as_binding_no_identifier{ "(a, b) as" };
	Example<pst::FlowPattern, false> flow_as_binding_keyword{ "_ as if" };
	Example<pst::FlowPattern, true>  flow_type_constraint_simple{ "x : i32" };
	Example<pst::FlowPattern, true>  flow_type_constraint_wildcard{ "_ : MyClass" };
	Example<pst::FlowPattern, true>  flow_type_constraint_variant{ "v : A | B" };
	Example<pst::FlowPattern, true>  flow_type_constraint_tuple{ "(a, b) : Point" };
	Example<pst::FlowPattern, false> flow_type_constraint_no_type{ "x :" };
	Example<pst::FlowPattern, false> flow_type_constraint_no_type_eof{ "x :" };
	Example<pst::FlowPattern, true>  flow_full_simple{ "x as my_var : i32" };
	Example<pst::FlowPattern, true>  flow_full_tuple{ "(a, b) as point : Point" };
	Example<pst::FlowPattern, true>  flow_full_deconstructor{ "Rectangle(w, h) as rect : Shape" };
	Example<pst::FlowPattern, true>  flow_full_wildcard{ "_ as value : A | B" };
	Example<pst::FlowPattern, true>  flow_subpattern_simple{ "Tuple(x as inner_x : i32, _)" };
	Example<pst::FlowPattern, true>  flow_subpattern_complex{
        "Response(200, Payload(u as user : User, _ as token : Token))"
	};
	Example<pst::FlowPattern, true>  flow_subpattern_in_tuple{ "(x : i32, y as coord_y : f64)" };
	Example<pst::FlowPattern, false> analysis_bad_value{ "+" };
	Example<pst::FlowPattern, false> analysis_binding_is_keyword{ "if" };

	// Example<pst::FlowPattern, false> flow_bad_order{ "x : i32 as my_var" };
	// Example<pst::FlowPattern, false> flow_double_as{ "x as var1 as var2" };
	// Example<pst::FlowPattern, false> flow_double_type{ "x : T1 : T2" };

	// TODOP: Fix block expressions.
	// Example<pst::ValuePattern, true> value_pattern_block_expression{ "{ x + y * z }" }; // TODOP:
	// Example<pst::ValuePattern, true> value_pattern_block_with_variable{ "{ my_constant }" };
	// Example<pst::FlowPattern, true>  analysis_block_expr{ "{ x + y * z }" }; // TODOP: Fix
	// Example<pst::FlowPattern, true>  analysis_block_with_variable{ "{ some_var }" }; // TODOP:
	// Example<pst::FlowPattern, true> analysis_literal_char{ "'c'" }; // TODOP: Fix

	void exampleTests() {
		for (auto e: examples) assertTrue((*e)(), e->message());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(exampleTests); }

public:
	~PSTErrorTests() override = default;
};

std::vector<PSTErrorTests::GenExample*> PSTErrorTests::examples = {};

TESTER_COMMON_MAIN("/src/compiler/core/pst_parser/tests/");
