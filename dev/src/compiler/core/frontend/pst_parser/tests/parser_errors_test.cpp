
#include <frontend/pst_parser/elements/elements_common.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/elements/implementations/class_elements/class_elements_errors.hpp>
#include <frontend/pst_parser/elements/implementations/declarations/declarations_errors.hpp>
#include <frontend/pst_parser/elements/implementations/declarations/var_parse.hpp>
#include <frontend/pst_parser/elements/implementations/expressions/expressions_errors.hpp>
#include <frontend/pst_parser/elements/implementations/lists/impl_template.hpp>
#include <frontend/pst_parser/elements/implementations/meta/meta_errors.hpp>
#include <frontend/pst_parser/elements/implementations/not_statements/not_statements_errors.hpp>
#include <frontend/pst_parser/elements/implementations/preamble.hpp>
#include <frontend/pst_parser/elements/implementations/statements/statements_errors.hpp>
#include <frontend/pst_parser/elements/parser_common_errors.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>

#include <diagnostic/source_position.hpp>
#include <tester/tester.hpp>

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

		template<typename Element, typename Parser>
		[[nodiscard]]
		static base::OkBad testCloning(const pst::PST<Element, Parser>& pst) {
			if (not pst.hasErrors()) {
				return pst::testElementCloning(
					base::CRef(&*pst.getRootElement().illegalAccess().value())
				);
			}
			return base::OK;
		}

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
			auto parsed = pst::PST<Element, Parser>::fromContents(code, pst::PSTType::Program);
			return ((not parsed.hasErrors()) == good && testCloning(parsed).isOk());
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

	template<bool good, typename Parser>
	struct Example<pst::CodeBlock, good, Parser>: public GenExample {
		Example(std::string code): GenExample(std::move(code)) {}

		bool operator()() override {
			auto parsed = pst::PST<pst::CodeBlock, Parser>::fromContentsWithArgs(
				code, pst::PSTType::Program, hashing::ComponentHash{}
			);
			return ((not parsed.hasErrors()) == good && testCloning(parsed).isOk());
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << code << "` as ";
			ss << base::typeName<pst::CodeBlock>();
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<bool good, typename Parser>
	struct Example<pst::CodeBlockOrStmt, good, Parser>: public GenExample {
		Example(std::string code): GenExample(std::move(code)) {}

		bool operator()() override {
			auto parsed = pst::PST<pst::CodeBlockOrStmt, Parser>::fromContentsWithArgs(
				code, pst::PSTType::Program, hashing::ComponentHash{}
			);
			return ((not parsed.hasErrors()) == good && testCloning(parsed).isOk());
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << code << "` as ";
			ss << base::typeName<pst::CodeBlockOrStmt>();
			ss << " expected parsing to " << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<std::derived_from<pst::Stmt> Element, bool good = true, typename Parser = Element>
	struct ClassStmtExample: public GenExample {
		base::StrID class_name;

		ClassStmtExample(std::string code): GenExample(std::move(code)), class_name("unnamed") {}

		ClassStmtExample(std::string code, const std::string& class_name):
			  GenExample(std::move(code)),
			  class_name(base::StrID(class_name.c_str())) {}

		bool operator()() override {
			auto parsed = pst::PST<Element, Parser>::fromContentsWithArgs(
				this->code,
				makeBox<pst::LangParserContext>(
					class_name, pst::BlockOrderType::Unordered, pst::StmtContext::Class
				),
				hashing::ComponentHash{}
			);
			return ((not parsed.hasErrors()) == good && testCloning(parsed).isOk());
		}

		[[nodiscard]]
		std::string message() const override {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << this->code << "` as ";
			ss << base::typeName<Element>();
			ss << " in class `" << class_name.str() << "`";
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
	Example<pst::Block, false> no_block{ "block" };
	Example<pst::Block, false> no_block_eof{ "block" };

	Example<pst::Stmt, false> not_all_parsed{ "call(3, 5 + 3 = 7);" };

	Example<pst::CodeBlockOrStmt, true> just_block{ "{}" };
	Example<pst::CodeBlockOrStmt, true> just_stmt{ "x=y;" };

	Example<pst::CodeBlock, true>  simple_code_block{ "{}" };
	Example<pst::CodeBlock, false> no_code_block{ "x=y;" };
	Example<pst::CodeBlock, false> no_code_block_eof{ "" };

	Example<pst::UniversalExprHolder, true> typed_literal1{ "100i32" };
	Example<pst::UniversalExprHolder, true> typed_literal4{ "3.14" };
	Example<pst::UniversalExprHolder, true> typed_literal7{ "1e-12f64" };
	Example<pst::UniversalExprHolder, true> typed_literal8{ "0b101001" };

	Example<pst::Const, true>  simple_const{ "const x: i32 = 5" };
	Example<pst::Const, true>  ref_const{ "const x: ref i32 = 5" };
	Example<pst::Const, true>  type_const{ "const x: i32" };
	Example<pst::Const, true>  value_const{ "const x = 5" };
	Example<pst::Const, false> no_name_const{ "const: i32 = 5" };
	Example<pst::Const, false> no_type_const{ "const x:= 5" };
	Example<pst::Const, false> no_value_const{ "const x: i32=" };
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
	Example<pst::Variable, false> no_value_var{ "var x: i32=" };
	Example<pst::Variable, false> no_value_var_eof{ "var x: i32=" };
	Example<pst::Variable, false> const_var{ "const x: i32 = 5" };

	Example<pst::DottedName, true>  simple_dotted{ "std.a.b.*" };
	Example<pst::DottedName, false> bad_dotted{ "std.a.b. .*" };

	Example<pst::Const, false> bad_stmt_choice{ "block {}" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder>           simple_expr{ "x + y" };
	Example<pst::ExprHolder, true, pst::UniversalAllowBlockExprHolder> block_expr{
		"x + {return 2 * x;}"
	};
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  unit_expr{ "()" };
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad_token_expr{ "\"" };

	Example<pst::Fun, true>      simple_function1{ "fun foo(x: i32, y: i32) -> (i32, i32) = {}" };
	Example<pst::Fun, true>      simple_function2{ "fun foo(x: i32, y: i32 = 1) = {}" };
	Example<pst::Fun, true>      simple_function3{ "fun foo(x: i32, y: i32 = 1,) = {}" };
	Example<pst::Fun, false>     bad_function{ "fun foo(x: i32, y) = {}" };
	Example<pst::FunDecl, true>  simple_fundecl1{ "fundecl foo(x: i32, y:i32) -> (i32, i32)" };
	Example<pst::FunDecl, true>  simple_fundecl2{ "fundecl foo()" };
	Example<pst::FunDecl, false> bad_fundecl1{ "fundecl foo(a)" };

	Example<pst::Fun, true>      operator_function1{ "fun +*(a: i64, b: i64) -> i64 = {}" };
	Example<pst::FunDecl, true>  operator_fundecl{ "fundecl +*(a: i64, b: i64) -> i64" };
	Example<pst::Class, true>    operator_method{ "class Foo { fun +*(a: u64) -> Foo = {} }" };
	Example<pst::Fun, true>      assignment_operator_function1{ "fun +*=(a: i64) = {}" };
	Example<pst::Fun, false>     bare_assign_operator_function{ "fun =(a: i64) = {}" };
	Example<pst::Fun, false>     comparison_operator_function1{ "fun <(a: i64) = {}" };
	Example<pst::Fun, false>     special_operator_function1{ "fun ->(a: i64) = {}" };
	Example<pst::Fun, false>     special_operator_function2{ "fun .?(a: i64) = {}" };
	Example<pst::FunDecl, false> reserved_operator_fundecl{ "fundecl ==(a: i64) -> i64" };

	Example<pst::Pattern, true> simple_pattern1{ "pattern IsEven(x: i32) = {}" };
	Example<pst::Pattern, true> simple_pattern2{
		"pattern Point(p: Point) -> (i32, i32) = {return (p.x, p.y);}"
	};
	Example<pst::Pattern, false> no_params_pattern{ "pattern Point() = {}" };
	Example<pst::Pattern, false> two_params_pattern{ "pattern Point(a: T, b: T) = {}" };
	Example<pst::Pattern, false> trailing_comma_pattern{ "pattern Point(a: T,) = {}" };

	Example<pst::If, true>  simple_if{ "if (a == b) {c = d;}" };
	Example<pst::If, true>  simple_if_else{ "if (a == b) {c = d;} else {c = e;}" };
	Example<pst::If, true>  simple_if_else_no_blocks{ "if (a == b) c = d; else c = e;" };
	Example<pst::If, false> empty_if_condition{ "if () {}" };
	Example<pst::If, true>  simple_const_if{ "if const (a == b) {c = d;}" };
	Example<pst::If, true>  const_if_else{ "if const (a == b) {c = d;} else {c = e;}" };
	Example<pst::If, true>  const_if_else_const_if{
        "if const (a == b) {c = d;} else if const (a == c) {c = e;}"
	};
	Example<pst::If, false> const_if_no_condition{ "if const {}" };

	Example<pst::Import, true>  simple_import{ "import std.math.sqrt as sqrt" };
	Example<pst::Import, false> empty_import{ "import" };
	Example<pst::Import, false> empty_nested_import{ "import ()" };
	Example<pst::Import, false> empty_star_import{ "import .*" };
	Example<pst::Import, true>  nested_import{ "import A.B.(C,)" };
	Example<pst::Import, true>  nested_import2{ "import A.B.(C,(D, E),)" };
	Example<pst::Import, false> missing_period_import{ "import A.B(C,(D, E),)" };
	Example<pst::Import, false> empty_nested_ard_import{ "import A.B.(C,(D, E),,)" };
	Example<pst::Import, true>  as_import{ "import A.B.C as D" };
	Example<pst::Import, true>  hides_import{ "import A.B.* hides D , G, C" };

	Example<pst::Namespace, true> simple_namespace{ "namespace name {}" };

	Example<pst::RoundGroupExpr, true>  simple_round_group{ "(a + b)" };
	Example<pst::RoundGroupExpr, false> bad_round_group{ "a + b" };

	Example<pst::Stmt, true> simple_stmt{ "x = a + b;" };
	Example<pst::Stmt, true> simple_stmt_implicit_return{ "x = a + b" };
	Example<pst::Stmt, true> expand_stmt{ "expand \"return 0;\";" };

	Example<pst::TopLevel, true> simple_top_level{ "fun foo() = {}" };

	Example<pst::Using, true> simple_using{ "using std.math" };

	Example<pst::Stmt, true>  public_specifier{ "public expand \"return 0;\"" };
	Example<pst::Stmt, true>  private_specifier{ "private fun foo() = {}" };
	Example<pst::Stmt, true>  protected_specifier{ "protected class x{}" };
	Example<pst::Stmt, true>  public_block{ "public {class x{}}" };
	Example<pst::Stmt, true>  extern_block{ "extern (\"C\") {class x{}}" };
	Example<pst::Stmt, true>  complex_block{ "public extern (\"C\") debug {class x{}}" };
	Example<pst::Stmt, true>  extern_block_two{ R"(extern ("C", "obj.o") {class x{}})" };
	Example<pst::Stmt, false> bad_specifier{ "def class x{};" };
	Example<pst::Stmt, false> empty_specifier{ "public;" };
	Example<pst::Stmt, false> bad_extern_block{ "extern {class x{}};" };

	Example<pst::UniversalExprHolder, true>  format1{ R"(f"{x} + {y} = {x + y}")" };
	Example<pst::UniversalExprHolder, true>  format2{ R"(f"{x}{y}{z}")" };
	Example<pst::UniversalExprHolder, true>  format3{ R"(f"nothing")" };
	Example<pst::UniversalExprHolder, true>  format4{ R"(f"{x}{y} = z")" };
	Example<pst::UniversalExprHolder, false> bad_format1{ R"(f"{;}")" };

	Example<pst::Stmt, true> trailing_comma_attr_arg_list{
		"@if_system(Windows,) print(\"windows\");"
	};
	Example<pst::Stmt, true> trailing_comma_call_list{ "print(\"windows\",);" };
	Example<pst::Stmt, true> trailing_comma_nested_import{ "import A.B.(C,)" };
	Example<pst::Stmt, true> trailing_comma_parameter_list{ "fun foo(a: A,) = {}" };
	Example<pst::Stmt, true> trailing_comma_template_list{ "x.y:{1,};" };

	Example<pst::For, true>  simple_for{ "for(a in a.b(x, y)) {}" };
	Example<pst::For, true>  simple_typed_for{ "for(a: T, U in a + c) {}" };
	Example<pst::For, false> empty_type_for{ "for(a: in a + c) {}" };
	Example<pst::For, false> no_in_for{ "for(a a + c) {}" };

	Example<pst::Class, true> simple_class{ "class x{}" };
	Example<pst::Class, true> complicated_class{ "class x extends y implements z:{}, d:{T} {}" };
	Example<pst::Class, true> nested_class{
		"class outer { class inner { x: i32 = 0; } x: i32 = 0;}"
	};
	Example<pst::Class, true> complicated_extends_class{ "class B extends ref A {}" };
	Example<pst::Class, true> complicated_extends_class2{ "class B extends 1 + 1 {}" };
	Example<pst::Class, true> complicated_implements_class{
		"class B extends A implements ref A, 1 + 1 {}"
	};
	Example<pst::Class, false> empty_extends_class{ "class x extends {}" };
	Example<pst::Class, false> empty_extends_class2{ "class x extends implements z {}" };
	Example<pst::Class, false> multiple_extends_class{ "class x extends y, z {}" };

	Example<pst::Expand, true>  nested_expand{ R"(expand "expand \"return 0;\";")" };
	Example<pst::Expand, false> empty_expand{ "expand ;" };
	Example<pst::Expand, false> unclosed_expand{ "expand \"return 0;;" };

	Example<pst::TemplateStmt, true>  simple_template{ "template () class C {}" };
	Example<pst::TemplateStmt, false> no_template_list{ "template class C {}" };
	Example<pst::TemplateStmt, false> no_statement{ "template ()" };

	ClassStmtExample<pst::Stmt, true> class_using{ "using std.math;" };
	ClassStmtExample<pst::Stmt, true> class_alias{ "alias sqrt=std.math.sqrt;" };

	ClassStmtExample<pst::Stmt, true> public_access_block{ "public {}" };
	ClassStmtExample<pst::Stmt, true> private_access_block{ "private {}" };
	ClassStmtExample<pst::Stmt, true> protected_access_block{ "protected {}" };
	ClassStmtExample<pst::Stmt, true> multi_specifier_block{ "public private {}" };
	ClassStmtExample<pst::Stmt, true> simple_specified_field{ "public static x: i32 = 5;" };

	ClassStmtExample<pst::Field, true>  simple_field{ "x: i32 = 5" };
	ClassStmtExample<pst::Field, true>  simple_var_field{ "var x: i32 = 5" };
	ClassStmtExample<pst::Field, true>  simple_let_field{ "let x: i32 = 5" };
	ClassStmtExample<pst::Field, true>  simple_let_field1{ "let x: i32" };
	ClassStmtExample<pst::Field, false> bad_field{ "x = 5" };
	ClassStmtExample<pst::Field, false> bad_field2{ "x : = 5" };

	ClassStmtExample<pst::Method, true> simple_method{
		"fun foo(x: i32, y: i32) -> (i32, i32) = {}"
	};

	ClassStmtExample<pst::Constructor, true> default_constructor{ "name(x: i32) = {}", "name" };
	ClassStmtExample<pst::Constructor, true> named_constructor{
		"name.from_pair(p: (i32, i32)) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> bad_constructor1{
		"name.(x: i32, y: i32): z(x, y) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> bad_constructor2{ "name.(x: i32, y: i32) -> i32 = {}",
		                                                        "name" };

	ClassStmtExample<pst::CopyConstructor, true> simple_copy_ctor{ "name.copy() = {}", "name" };

	ClassStmtExample<pst::MoveConstructor, true> simple_move_ctor{ "name.move() = {}", "name" };

	ClassStmtExample<pst::Destructor, true>  simple_destructor{ "name.destroy() = {}", "name" };
	ClassStmtExample<pst::Destructor, false> non_empty_destructor{ "name.destroy(x: i32) = {}",
		                                                           "name" };

	// @todo Some weird position bug for later
	// Example<pst::Variable, true> simpleVariable{"var x: i32 = 5"};
	// Example<pst::Variable, true> simpleLetVariable{"let x: i32 = 5"};

	Example<pst::While, true> simple_while{ "while (x < 5) {}" };

	Example<pst::UniversalExprHolder, true>  simple_ternary{ "if 5 then '\\n' else y" };
	Example<pst::UniversalExprHolder, false> bad1_ternary{ "if if 5 then x else y" };
	Example<pst::UniversalExprHolder, false> bad2_ternary{ "+ if 5 then x else y" };
	Example<pst::UniversalExprHolder, false> bad3_ternary{ "if 5 else y" };

	Example<pst::UniversalExprHolder, true> range_operator{ "x[1 .. 10]" };
	Example<pst::UniversalExprHolder, true> range_operator_to{ "x[.. 10]" };
	Example<pst::UniversalExprHolder, true> range_operator_from{ "x[1 ..]" };
	// This is because it's lexed as two floats 1. and .10, not necessarily desired behaviour
	Example<pst::UniversalExprHolder, false> range_operator_bad{ "x[1..10]" };

	Example<pst::ExprStmt, true>  simple_assign{ "x = y" };
	Example<pst::ExprStmt, true>  simple_string_assign{ "x = \"left\"" };
	Example<pst::ExprStmt, false> bad_assign{ "x = y = z" };
	Example<pst::ExprStmt, false> bad_operator{ "x = y z + 3" };

	Example<pst::UniversalExprHolder, true> simple_operators{ "++ ++ 3 + 5 ++" };
	Example<pst::UniversalExprHolder, true> text_operator{ "++ ++ 3 + 5 kg ++" };
	Example<pst::UniversalExprHolder, true> new_operators{ "<> 3 <> 'x' <>" };
	Example<pst::UniversalExprHolder, true> all_integer_operators{ "1 + 2 - 3 * 4 / 5 % 6 ** 7" };
	Example<pst::UniversalExprHolder, true> all_boolean_operators{
		"true and true or false and not false"
	};
	Example<pst::UniversalExprHolder, true>  prefix_named{ "ref const T.Y" };
	Example<pst::UniversalExprHolder, false> bad_operators{ "++ ++ ++ ++" };

	Example<pst::UniversalAllowBlockExprHolder, true> simple_block_expr{
		"x::size() + {return 2;}"
	};

	Example<pst::UniversalExprHolder, true> simple_round_expr{ "x.?y + (x, y)" };

	Example<pst::UniversalExprHolder, true> simple_chain_expr{ "(x * t).y.z(4)[3]" };

	Example<pst::UniversalExprHolder, true> simple_template_expr{
		"(x * t).y:{x, y}::z:{abc}(4)[3]"
	};

	Example<pst::FlowPattern, true> flow_tuple_simple{ "(1, x)" };
	Example<pst::FlowPattern, true> flow_tuple_nested{ "(1, (x, _))" };
	Example<pst::FlowPattern, true> flow_deconstructor_simple{ "Point(x, y)" };
	Example<pst::FlowPattern, true> flow_deconstructor_nested{ "Circle(Point(0, 0), r)" };
	Example<pst::FlowPattern, true> flow_as_binding{ "Rectangle(a, b) as rect" };
	Example<pst::FlowPattern, true> flow_type_constraint_simple{ "x : A | B" };
	Example<pst::FlowPattern, true> flow_type_constraint_tuple{ "(a, b) : Point" };
	Example<pst::FlowPattern, true> flow_full_deconstructor{ "Rectangle(w, h) as rect : Shape" };
	Example<pst::FlowPattern, true> flow_full_wildcard{ "_ as value : A | B" };
	Example<pst::FlowPattern, true> flow_subpattern_complex{
		"Response(200, Payload(u as user : User, _ as token : Token))"
	};
	Example<pst::FlowPattern, true>  flow_subpattern_in_tuple{ "(x : i32, y as coord_y : f64)" };
	Example<pst::FlowPattern, true>  flow_block_expr{ "{ x + y * z; }" };
	Example<pst::FlowPattern, false> flow_tuple_empty{ "()" };
	Example<pst::FlowPattern, false> flow_tuple_unclosed{ "(1, x" };
	Example<pst::FlowPattern, false> flow_deconstructor_empty_args{ "None()" };
	Example<pst::FlowPattern, false> flow_deconstructor_unclosed{ "Point(x, y" };
	Example<pst::FlowPattern, false> flow_as_binding_no_identifier{ "(a, b) as" };
	Example<pst::FlowPattern, false> flow_as_binding_keyword{ "_ as if" };
	Example<pst::FlowPattern, false> flow_type_constraint_no_type{ "x :" };

	Example<pst::AssignmentExprHolder, true> match_big{
		R"(match (x) {
        case 1                  = print("one");
        case "kajak"            = 1;
        case my_var             = true; 
        case true              	= { let y = x + 1; return y; };
        case (0, 0)             = false;
        case (_, 0)             = print("four");
        case { x == true; }     = print("five");
        case Even(x)            = print("six");
        case Rectangle(_, _) as colorful : Colorful = print("seven");
        case Y(X(x1, x2) as x, y)                   = print("eight");
        case (a, b) if a > 0 and b > 0              = print("nine");
                    if a < 0 and b < 0              = print("ten");
        case _ : A | C                              = print("eleven");
        case _                                      = print("did not match");
    })"
	};
	Example<pst::AssignmentExprHolder, true>  match_no_cases_in_block{ R"(match (value) {})" };
	Example<pst::AssignmentExprHolder, false> match_no_value_expr{ R"(match { case _ = 1; })" };
	Example<pst::AssignmentExprHolder, false> match_no_parens_for_value{
		R"(match x { case _ = 1; })"
	};
	Example<pst::AssignmentExprHolder, false> match_empty_parens_for_value{
		R"(match () { case _ = 1; })"
	};
	Example<pst::AssignmentExprHolder, false> match_no_curly_braces{ R"(match(x))" };
	Example<pst::AssignmentExprHolder, false> match_unclosed_curly_braces{
		R"(match(x) { case _ = 1)"
	};
	Example<pst::AssignmentExprHolder, false> match_case_no_pattern{ R"(match(x) { case = 1; })" };
	Example<pst::AssignmentExprHolder, false> match_case_no_body{ R"(match(x) { case 1; })" };
	Example<pst::AssignmentExprHolder, false> match_case_no_equals{
		R"(match(x) { case 1 "one"; })"
	};
	Example<pst::AssignmentExprHolder, false> match_case_if_guard_no_condition{
		R"(match(x) { case _ if = 1; })"
	};
	Example<pst::AssignmentExprHolder, false> match_case_if_guard_no_body{
		R"(match(x) { case _ if x > 0; })"
	};
	Example<pst::AssignmentExprHolder, false> match_if_after_default_branch{
		R"(match(x) { case _ = 1; if x > 10 = 2; })"
	};
	Example<pst::AssignmentExprHolder, false> match_default_branch_after_if{
		R"(match(x) { case _ if x > 10 = 2; = 2; })"
	};
	Example<pst::AssignmentExprHolder, false> match_junk_between_cases{
		R"(match(x) { case 1 = "one"; let y = 5; case 2 = "two"; })"
	};

	void exampleTests() {
		for (auto e: examples) {
			std::println(std::cerr, "{}", e->code);
			assertTrue((*e)(), e->message(), false);
		}
	}

	void diagnosticTests() {
		using dia::testDiagnosticMessage;
		std::stringstream ss;

		testDiagnosticMessage<pst::error::BlockStartError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::error::DuplicateSemicolon>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::BadStatementChoice<pst::Alias>>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::NonEmptyError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::NoSpecifierError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::PatternArgumentCountError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::PatternBracketError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::ForBracketError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::VariableNoTypeAndValueError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::BadValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MoreThanValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadUnitExprError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MultipleTernaryError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::PartialTernaryError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::ImproperTernaryError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadTemplateError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadStrValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MoreThanStrValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadRoundExprError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MatchRoundBracketError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::NotACaseExpression>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MatchCurlyBracketError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::OnlyPrefixError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadCharValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MoreThanCharValueError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadChainExprError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadCallError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadBlockError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::NoAtomError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MultipleAssignmentError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadAccessError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::EmptyStatementError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::UnrecognizedPatternInCaseError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::RoundExprStartError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::MatchCaseWithNoBodyError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::DoubleDefaultBranchError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::UnconditionedBranchAfterConditionedError>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::AttrStarError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::EmptyExprError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::AliasStarError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::BadSpecifierCallError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::InvalidExternContentWarning>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::NoExternArgumentError>(ss, dia::SourcePosition::fakePosition());
		testDiagnosticMessage<pst::ReservedOperatorFunNameError>(
			ss, dia::SourcePosition::fakePosition(), std::string("==")
		);
		testDiagnosticMessage<pst::AssignmentOperatorFunNameError>(
			ss, dia::SourcePosition::fakePosition(), std::string("+*=")
		);

		testDiagnosticMessage<
			pst::OpeningBracketMissingError<pst::internal::NameGetters::inheritanceList>>(
			ss, dia::SourcePosition::fakePosition(), lexer::Token::BracketType::Round
		);
		testDiagnosticMessage<pst::EmptyListError<pst::internal::NameGetters::inheritanceList>>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::EmptyListElementError<pst::internal::NameGetters::inheritanceList>>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::EmptyFieldError<pst::internal::NameGetters::inheritanceList>>(
			ss, dia::SourcePosition::fakePosition()
		);
		testDiagnosticMessage<pst::NoSeparatorError<pst::internal::NameGetters::inheritanceList>>(
			ss, dia::SourcePosition::fakePosition()
		);
		std::cerr << ss.str();
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(exampleTests);
		TESTER_ADD_TEST(diagnosticTests);
	}

public:
	~PSTErrorTests() override = default;
};

std::vector<PSTErrorTests::GenExample*> PSTErrorTests::examples = {};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
