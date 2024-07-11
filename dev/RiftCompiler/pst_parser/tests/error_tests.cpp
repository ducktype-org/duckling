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
		GenExample() { examples.push_back(this); }

		virtual bool operator()() = 0;
		[[nodiscard]]
		virtual std::string message() const
			= 0;

		virtual ~GenExample() = default;
	};

	template<typename Element, bool good = true>
	struct Example: public GenExample {
		std::string code;

		Example(std::string code): GenExample(), code(std::move(code)) {}

		bool operator()() override {
			auto parsed = pst::PST<Element>::fromContents(code);
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

	template<typename Element, bool good>
	void testExample(Example<Element, good>& example) {
		assert(example(), example.message());
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

	Example<pst::Expr, true>  simpleExpr{ "x + y" };
	Example<pst::Expr, false> badTokenExpr{ "\"" };

	void exampleTests() {
		for (auto e: examples) assert((*e)(), e->message());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("PST Error Tests") {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(exampleTests);
	}

public:
	~PSTErrorTests() override = default;
};

std::vector<PSTErrorTests::GenExample*> PSTErrorTests::examples = {};

TESTER_COMMON_MAIN("/RiftCompiler/pst_parser/tests/");
