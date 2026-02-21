#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <filesystem/file.hpp>
#include <tester/tester.hpp>

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
			auto parsed = pst::PST<Element>::fromContents(code, pst::PSTType::Program);
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

	Example<pst::TopLevel, true> empty_top_level{ "" };
	Example<pst::Block, true>    empty_block{ "block {}" };
	Example<pst::While, false>   bad_choice{ "block {}" };
	Example<pst::Block, false>   no_brackets{ "const x:i32=3;" };

	void exampleTests() {
		testExample(empty_top_level);
		testExample(empty_block);
		testExample(bad_choice);
		testExample(no_brackets);
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(equivalencyTest<pst::TopLevel>);
		TESTER_ADD_TEST(exampleTests);
	}

private:
	template<typename Element>
	pst::PST<Element> manualSteps(const std::string& filename) {
		auto file = tokenizer::makeTokenSource(fs::File(filename));
		file->tokenize();
		return { std::move(file), pst::PSTType::Program };
	}

	template<typename Element>
	pst::PST<Element> fromContents(const std::string& filename) {
		std::string contents{ fs::File(filename).getContent().view().stringView() };
		return pst::PST<Element>::fromContents(contents, pst::PSTType::Program);
	}

	template<typename Element>
	pst::PST<Element> fromFilename(const std::string& filename) {
		return { fs::File(filename), pst::PSTType::Program };
	}

	template<typename Element>
	std::string stringDprint(const pst::PST<Element>& pst) {
		std::stringstream ss;
		pst.dprint(ss);
		return ss.str();
	}

	template<typename Element>
	void singleEquivalency(const std::string& local_path) {
		const std::string error        = "outputs from parsing on file " + local_path + "differ.";
		const std::string filepath     = path(local_path);
		pst::PST<Element> pst_manual   = manualSteps<Element>(filepath);
		pst::PST<Element> pst_content  = fromContents<Element>(filepath);
		pst::PST<Element> pst_filename = fromFilename<Element>(filepath);
		assertTrue(pst_manual.getLogger()->good() == pst_content.getLogger()->good(), error);
		assertTrue(pst_manual.getLogger()->good() == pst_filename.getLogger()->good(), error);
		std::string manual_print   = stringDprint(pst_manual);
		std::string content_print  = stringDprint(pst_content);
		std::string filename_print = stringDprint(pst_filename);
		assertTrue(manual_print == content_print, error);
		assertTrue(manual_print == filename_print, error);
	}

	template<typename Element>
	void equivalencyTest() {
		for (const auto& local_path: paths) singleEquivalency<Element>(local_path);
	}

public:
	~PSTBuilderTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/frontend/pst_parser/tests/");
