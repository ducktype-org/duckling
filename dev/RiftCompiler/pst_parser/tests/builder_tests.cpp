#include <filesystem/file.hpp>
#include <fstream>
#include <iostream>
#include <pst_parser/parser.hpp>
#include <pst_parser/pst_builder.hpp>
#include <pst_parser/pst_visitor.hpp>


#include <lexer/lexer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

class PSTBuilderTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PSTBuilderTest

	std::vector<std::string> paths = {
		"snippets/actions.rift",
		"snippets/all_statements.txt",
		"snippets/block.rift",
		"snippets/fun.rift",
		"snippets/fun2.rift",
		"snippets/if.rift",
		"snippets/import.rift",
		"snippets/lists_err.rift",
		"snippets/lists_ok.rift",
		"snippets/missing_semicolon_err.rift",
		"snippets/namespace.rift",
		"snippets/params_err.rift",
		"snippets/struct.rift",
		"snippets/using_err.rift",
		"snippets/using.rift",
		"snippets/while.rift",
	};

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("PST Builder Tests") {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(equivalencyTest<pst::TopLevel>);
		TESTER_ADD_TEST(equivalencyTest<pst::CodeBlockOrStmt>);
	}

private:
	template<typename Element>
	pst::PSTBuilder<Element> manualSteps(const std::string& filename) {
		auto file = tokenizer::makeTokenFile(fs::FilePath(filename));
		file->tokenize();
		return { std::move(file) };
	}

	template<typename Element>
	pst::PSTBuilder<Element> fromContents(const std::string& filename) {
		std::string contents{fs::getSimpleFileContent(filename).view().stringView()};
		return pst::PSTBuilder<Element>::fromContents(contents);
	}

	template<typename Element>
	pst::PSTBuilder<Element> fromFilename(const std::string& filename) {
		return { fs::FilePath(filename) };
	}
	
	template <typename Element>
	std::string stringDprint(const pst::PSTBuilder<Element>& pst) {
		std::stringstream ss;
		pst.dprint(ss);
		return ss.str();
	}

	template <typename Element>
	void signgleEquivalency(const std::string& local_path) {
		const std::string error = "outputs from parsing on file " + local_path + "differ.";
		const std::string filepath = path(local_path);
		pst::PSTBuilder<Element> PSTmanual = manualSteps<Element>(filepath);
		pst::PSTBuilder<Element> PSTcontent = fromContents<Element>(filepath);
		pst::PSTBuilder<Element> PSTfilename = fromFilename<Element>(filepath);
		assert(PSTmanual.getLogger().good() == PSTcontent.getLogger().good(), error);
		assert(PSTmanual.getLogger().good() == PSTfilename.getLogger().good(), error);
		std::string manualPrint = stringDprint(PSTmanual);
		std::string contentPrint = stringDprint(PSTcontent);
		std::string filenamePrint = stringDprint(PSTfilename);
		assert(manualPrint == contentPrint, error);
		assert(manualPrint == filenamePrint, error);
	}

	template <typename Element>
	void equivalencyTest() {
		for(const auto& local_path: paths) {
			signgleEquivalency<Element>(local_path);
		}
	}

public:
	~PSTBuilderTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/pst_parser/tests/");
