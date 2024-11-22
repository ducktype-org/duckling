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

	Example<pst::Alias, true>  simpleAlias{ "alias sqrt=std.math.sqrt" };
	Example<pst::Alias, false> aliasStar{ "alias math=std.math.*" };

	Example<pst::Attribute, true> simpleAttr{ "@pretty(5)" };

	Example<pst::Block, true>  simpleBlock{ "block {}" };
	Example<pst::Block, false> noBlock{ "block;" };
	Example<pst::Block, false> noBlockEof{ "block" };

	Example<pst::CodeBlockOrStmt, true> justBlock{ "{}" };
	Example<pst::CodeBlockOrStmt, true> justStmt{ "x=y;" };

	Example<pst::CodeBlock, true>  simpleCodeBlock{ "{}" };
	Example<pst::CodeBlock, false> noCodeBlock{ "x=y;" };
	Example<pst::CodeBlock, false> noCodeBlockEof{ "" };

	Example<pst::Const, true>  simpleConst{ "const x: i32 = 5" };
	Example<pst::Const, false> noNameConst{ "const: i32 = 5" };
	Example<pst::Const, false> noTypeConst{ "const x:= 5" };
	Example<pst::Const, false> noValueConst{ "const x: i32=;" };
	Example<pst::Const, false> noValueConstEof{ "const x: i32=" };
	Example<pst::Const, false> noEqualsConst{ "const x: i32" };

	Example<pst::DottedName, true>  simpleDotted{ "std.a.b.*;" };
	Example<pst::DottedName, false> badDotted{ "std.a.b. .*" };

	Example<pst::Const, false> badStmtChoice{ "block {}" };

	Example<pst::ExprElement, true, pst::UniversalExpr>  simpleExpr{ "x + y" };
	Example<pst::ExprElement, true, pst::UniversalExpr>  blockExpr{ "x + {return 2 * x;}" };
	Example<pst::ExprElement, false, pst::UniversalExpr> badTokenExpr{ "\"" };

	Example<pst::Fun, true>  simpleFunction1{ "fun foo(x: i32, y: i32) -> (i32, i32) = {}" };
	Example<pst::Fun, true>  simpleFunction2{ "fun foo(x: i32, y: i32 = 1) = {}" };
	Example<pst::Fun, false> badFunction{ "fun foo(x: i32, y) = {}" };

	Example<pst::If, true> simpleIf{ "if (a == b) {c = d;}" };
	Example<pst::If, true> simpleIfElse{ "if (a == b) {c = d;} else {c = e;}" };
	Example<pst::If, true> simpleIfElseNoBlocks{ "if (a == b) c = d; else c = e;" };

	Example<pst::Import, true> simpleImport{ "import std.math.sqrt as sqrt" };

	Example<pst::Namespace, true> simpleNamespace{ "namespace name {}" };

	Example<pst::RoundGroupExpr, true>  simpleRoundGroup{ "(a + b)" };
	Example<pst::RoundGroupExpr, false> badRoundGroup{ "a + b" };

	Example<pst::Stmt, true>  simpleStmt{ "x = a + b;" };
	Example<pst::Stmt, false> badStmt{ "x = a + b" };

	Example<pst::TopLevel, true> simpleTopLevel{ "fun foo() = {}" };

	Example<pst::Using, true> simpleUsing{ "using std.math" };

	Example<pst::For, true>  simpleFor{ "for(a in a.b(x, y)) {}" };
	Example<pst::For, true>  simpleTypedFor{ "for(a: T, U in a + c) {}" };
	Example<pst::For, false> emptyTypeFor{ "for(a: in a + c) {}" };
	Example<pst::For, false> noInFor{ "for(a a + c) {}" };

	Example<pst::Class, true>  simpleClass{ "class x{}" };
	Example<pst::Class, true>  complicatedClass{ "class x extends y implements z:{}, d:{T} {}" };
	Example<pst::Class, false> emptyExtendsClass{ "class x extends {}" };
	Example<pst::Class, false> emptyExtendsClass2{ "class x extends implements z {}" };
	Example<pst::Class, false> multipleExtendsClass{ "class x extends y, z {}" };

	ClassStmtExample<pst::AccessBlock, true>  publicAccessBlock{ "public {}" };
	ClassStmtExample<pst::AccessBlock, true>  privateAccessBlock{ "private {}" };
	ClassStmtExample<pst::AccessBlock, true>  protectedAccessBlock{ "protected {}" };
	ClassStmtExample<pst::AccessBlock, false> multiSpecifierBlock{ "public private {}" };

	ClassStmtExample<pst::Field, true>  simpleField{ "x: i32 = 5" };
	ClassStmtExample<pst::Field, true>  simpleSpecifiedField{ "public static x: i32 = 5" };
	ClassStmtExample<pst::Field, false> badField{ "x = 5" };
	ClassStmtExample<pst::Field, false> badField2{ "x : = 5" };

	ClassStmtExample<pst::Method, true> simpleMethod{
		"fun foo(x: i32, y: i32) -> (i32, i32) = {}"
	};

	ClassStmtExample<pst::Constructor, true> defaultConstructor{ "name(x: i32) = {}", "name" };
	ClassStmtExample<pst::Constructor, true> namedConstructor{ "name.from_pair(p: (i32, i32)) = {}",
		                                                       "name" };
	ClassStmtExample<pst::Constructor, true> initConstructor{
		"name.init(x: i32, y: i32): z(x, y) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> badConstructor1{
		"name.(x: i32, y: i32): z(x, y) = {}", "name"
	};
	ClassStmtExample<pst::Constructor, false> badConstructor2{ "name.(x: i32, y: i32) -> i32 = {}",
		                                                       "name" };

	ClassStmtExample<pst::Destructor, true>  simpleDestructor{ "name.destroy() = {}", "name" };
	ClassStmtExample<pst::Destructor, false> nonEmptyDestructor{ "name.destroy(x: i32) = {}",
		                                                         "name" };

	// @todo Some weird position bug for later
	// Example<pst::Variable, true> simpleVariable{"var x: i32 = 5"};
	// Example<pst::Variable, true> simpleLetVariable{"let x: i32 = 5"};

	Example<pst::While, true> simpleWhile{ "while (x < 5) {}" };

	void exampleTests() {
		for (auto e: examples) assertTrue((*e)(), e->message());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(exampleTests);
	}

public:
	~PSTErrorTests() override = default;
};

std::vector<PSTErrorTests::GenExample*> PSTErrorTests::examples = {};

TESTER_COMMON_MAIN("/compiler/pst_parser/tests/");
