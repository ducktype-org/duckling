#include <filesystem/file.hpp>
#include <pst_parser/pst.hpp>
#include <pst_parser/pst_visitor.hpp>


#include <lexer/lexer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

class PSTBuilderTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PSTBuilderTest

	std::vector<std::string> paths = {
		"snippets/actions.rift",   "snippets/all_statements.txt",
		"snippets/block.rift",     "snippets/fun.rift",
		"snippets/fun2.rift",      "snippets/if.rift",
		"snippets/import.rift",    "snippets/lists_err.rift",
		"snippets/lists_ok.rift",  "snippets/missing_semicolon_err.rift",
		"snippets/namespace.rift", "snippets/params_err.rift",
		"snippets/class.rift",     "snippets/using_err.rift",
		"snippets/using.rift",     "snippets/while.rift",
	};

	template<typename Element, bool good = true>
	struct Example {
		std::string code;

		bool operator()() {
			auto parsed = pst::PST<Element>::fromContents(code);
			return parsed.getLogger().good() == good;
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
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(equivalencyTest<pst::TopLevel>);
		TESTER_ADD_TEST(equivalencyTest<pst::CodeBlockOrStmt>);
		TESTER_ADD_TEST(exampleTests);
	}

private:
	template<typename Element>
	pst::PST<Element> manualSteps(const std::string& filename) {
		auto file = tokenizer::makeTokenFile(fs::FilePath(filename));
		file->tokenize();
		return { std::move(file) };
	}

	template<typename Element>
	pst::PST<Element> fromContents(const std::string& filename) {
		std::string contents{ fs::getSimpleFileContent(filename).view().stringView() };
		return pst::PST<Element>::fromContents(contents);
	}

	template<typename Element>
	pst::PST<Element> fromFilename(const std::string& filename) {
		return { fs::FilePath(filename) };
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
		assertTrue(PSTmanual.getLogger().good() == PSTcontent.getLogger().good(), error);
		assertTrue(PSTmanual.getLogger().good() == PSTfilename.getLogger().good(), error);
		std::string manualPrint   = stringDprint(PSTmanual);
		std::string contentPrint  = stringDprint(PSTcontent);
		std::string filenamePrint = stringDprint(PSTfilename);
		assertTrue(manualPrint == contentPrint, error);
		assertTrue(manualPrint == filenamePrint, error);
	}

	template<typename Element>
	void equivalencyTest() {
		for (const auto& local_path: paths) signgleEquivalency<Element>(local_path);
	}

public:
	~PSTBuilderTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/pst_parser/tests/");
