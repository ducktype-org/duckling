#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <pst_parser/pst_visitor.hpp>


#include <lexer/lexer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
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
			return parsed.getLogger().good() == good;
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
			return parsed.getLogger().good() == good;
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
	Example<pst::Const, true> ref_const{ "const x: ref i32 = 5" };
	Example<pst::Const, false> no_name_const{ "const: i32 = 5" };
	Example<pst::Const, false> no_type_const{ "const x:= 5" };
	Example<pst::Const, false> no_value_const{ "const x: i32=;" };
	Example<pst::Const, false> no_value_const_eof{ "const x: i32=" };
	Example<pst::Const, false> no_equals_const{ "const x: i32" };

	Example<pst::DottedName, true>  simple_dotted{ "std.a.b.*;" };
	Example<pst::DottedName, false> bad_dotted{ "std.a.b. .*" };

	Example<pst::Const, false> bad_stmt_choice{ "block {}" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  simple_expr{ "x + y" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  block_expr{ "x + {return 2 * x;}" };
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad_token_expr{ "\"" };

	Example<pst::Fun, true>  simple_function1{ "fun foo(x: i32, y: i32) -> (i32, i32) = {}" };
	Example<pst::Fun, true>  simple_function2{ "fun foo(x: i32, y: i32 = 1) = {}" };
	Example<pst::Fun, false> bad_function{ "fun foo(x: i32, y) = {}" };

	Example<pst::If, true> simple_if{ "if (a == b) {c = d;}" };
	Example<pst::If, true> simple_if_else{ "if (a == b) {c = d;} else {c = e;}" };
	Example<pst::If, true> simple_if_else_no_blocks{ "if (a == b) c = d; else c = e;" };

	Example<pst::Import, true> simple_import{ "import std.math.sqrt as sqrt" };

	Example<pst::Namespace, true> simple_namespace{ "namespace name {}" };

	Example<pst::RoundGroupExpr, true>  simple_round_group{ "(a + b)" };
	Example<pst::RoundGroupExpr, false> bad_round_group{ "a + b" };

	Example<pst::Stmt, true>  simple_stmt{ "x = a + b;" };
	Example<pst::Stmt, false> bad_stmt{ "x = a + b" };

	Example<pst::TopLevel, true> simple_top_level{ "fun foo() = {}" };

	Example<pst::Using, true> simple_using{ "using std.math" };

	Example<pst::For, true>  simple_for{ "for(a in a.b(x, y)) {}" };
	Example<pst::For, true>  simple_typed_for{ "for(a: T, U in a + c) {}" };
	Example<pst::For, false> empty_type_for{ "for(a: in a + c) {}" };
	Example<pst::For, false> no_in_for{ "for(a a + c) {}" };

	Example<pst::Class, true>  simple_class{ "class x{}" };
	Example<pst::Class, true>  complicated_class{ "class x extends y implements z:{}, d:{T} {}" };
	Example<pst::Class, false> empty_extends_class{ "class x extends {}" };
	Example<pst::Class, false> empty_extends_class2{ "class x extends implements z {}" };
	Example<pst::Class, false> multiple_extends_class{ "class x extends y, z {}" };

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

	Example<pst::ExprHolder, true, pst::UniversalExprHolder> simple_ternary{ "if 5 then x else y" };
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad1_ternary{
		"if if 5 then x else y"
	};
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad2_ternary{
		"+ if 5 then x else y"
	};
	Example<pst::ExprHolder, false, pst::UniversalExprHolder> bad3_ternary{ "if 5 else y" };

	Example<pst::ExprStmt, true>  simple_assign{ "x = y" };
	Example<pst::ExprStmt, false> bad_assign{ "x = y = z" };

	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  simple_operators{ "++ ++ 3 + 5 ++" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  text_operator{ "++ ++ 3 + 5 kg ++" };
	Example<pst::ExprHolder, true, pst::UniversalExprHolder>  new_operators{ "<> 3 <> 5 <>" };
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

	void exampleTests() {
		for (auto e: examples) assertTrue((*e)(), e->message());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(exampleTests); }

public:
	~PSTErrorTests() override = default;
};

std::vector<PSTErrorTests::GenExample*> PSTErrorTests::examples = {};

TESTER_COMMON_MAIN("/compiler/pst_parser/tests/");
