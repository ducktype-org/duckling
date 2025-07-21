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

class PSTBuilderTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PSTBuilderTest

	std::vector<std::string> paths = {
		"snippets/actions.duck",     "snippets/all_statements.txt",
		"snippets/block.duck",       "snippets/fun.duck",
		"snippets/fun2.duck",        "snippets/if.duck",
		"snippets/import.duck",      "snippets/lists_err.duck",
		"snippets/lists_ok.duck",    "snippets/missing_semicolon_err.duck",
		"snippets/namespace.duck",   "snippets/params_err.duck",
		"snippets/class.duck",       "snippets/using_err.duck",
		"snippets/using.duck",       "snippets/while.duck",
		"snippets/expressions.duck",
	};

	template<typename Element, bool good = true>
	struct Example {
		std::string code;

		bool operator()() {
			auto parsed = pst::PST<Element>::fromContents(code);
			return parsed.getLogger()->good() == good;
		}

		[[nodiscard]]
		std::string message() const {
			std::stringstream ss;
			ss << "Unexpected behaviour while parsing: `" << code << "` as ";
			ss << base::typeName<Element>();
			ss << " expected parsing to" << (good ? "succeed" : "fail") << ".";
			return ss.str();
		}
	};

	template<typename Element, bool good>
	void testExample(Example<Element, good>& example) {
		assertTrue(example(), example.message());
	}

	Example<pst::TopLevel, true>   emptyTopLevel{ "" };
	Example<pst::Block, true>      emptyBlock{ "block {}" };
	Example<pst::While, false>     badChoice{ "block {}" };
	Example<pst::CodeBlock, false> noBrackets{ "const x:i32=3;" };

	void exampleTests() {
		testExample(emptyTopLevel);
		testExample(emptyBlock);
		testExample(badChoice);
		testExample(noBrackets);
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(equivalencyTest<pst::TopLevel>);
		TESTER_ADD_TEST(equivalencyTest<pst::CodeBlockOrStmt>);
		TESTER_ADD_TEST(exampleTests);
	}

private:
	template<typename Element>
	pst::PST<Element> manualSteps(const std::string& filename) {
		auto file = tokenizer::makeTokenSource(fs::File(filename));
		file->tokenize();
		return { std::move(file) };
	}

	template<typename Element>
	pst::PST<Element> fromContents(const std::string& filename) {
		std::string contents{ fs::File(filename).getContent().view().stringView() };
		return pst::PST<Element>::fromContents(contents);
	}

	template<typename Element>
	pst::PST<Element> fromFilename(const std::string& filename) {
		return { fs::File(filename) };
	}

	template<typename Element>
	std::string stringDprint(const pst::PST<Element>& pst) {
		std::stringstream ss;
		pst.dprint(ss);
		return ss.str();
	}

	template<typename Element>
	void signgleEquivalency(const std::string& local_path) {
		const std::string error       = "outputs from parsing on file " + local_path + "differ.";
		const std::string filepath    = path(local_path);
		pst::PST<Element> PSTmanual   = manualSteps<Element>(filepath);
		pst::PST<Element> PSTcontent  = fromContents<Element>(filepath);
		pst::PST<Element> PSTfilename = fromFilename<Element>(filepath);
		assertTrue(PSTmanual.getLogger()->good() == PSTcontent.getLogger()->good(), error);
		assertTrue(PSTmanual.getLogger()->good() == PSTfilename.getLogger()->good(), error);
		std::string manual_print   = stringDprint(PSTmanual);
		std::string content_print  = stringDprint(PSTcontent);
		std::string filename_print = stringDprint(PSTfilename);
		assertTrue(manual_print == content_print, error);
		assertTrue(manual_print == filename_print, error);
	}

	template<typename Element>
	void equivalencyTest() {
		for (const auto& local_path: paths) signgleEquivalency<Element>(local_path);
	}

public:
	~PSTBuilderTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/pst_parser/tests/");
