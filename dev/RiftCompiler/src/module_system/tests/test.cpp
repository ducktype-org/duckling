#include <lexer/lexer.hpp>
#include <module_system/import_to_path.hpp>
#include <pst_parser/parser.hpp>
#include <tester/tester.hpp>

class ModuleSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ModuleSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Module system test") {
		lexer::init();
		pst::init();

		TESTER_ADD_TEST(testImportToPath);
	}

private:
	pst::PST prepare(const std::string &filename) {
		fs::FilePath file(filename);
		auto         td = lexer::tokenizeFile(file);
		return pst::parse(std::move(td));
	}

	void testSingleImportToPath(
		const pst::Import &import, std::string local_path, std::string expected_output
	) {
		assert(
			modulesys::importToPath(local_path, import) == expected_output,
			"import incorrect - got " + modulesys::importToPath(local_path, import) + ", expected "
				+ expected_output,
			false
		);
	}

	void testImportToPath() {
		pst::PST pst     = prepare(path("snippets/import.rift"));
		auto     imports = pst.getImports();
		assert(imports.size() == 6, "wrong number of imports");
		testSingleImportToPath(*imports[0], "/", "/aaaa/bbb/c.rift");
		testSingleImportToPath(*imports[1], "./", "./dasdsa/dasd/*.rift");
		testSingleImportToPath(*imports[2], "src/compiler", "src/compiler/aaaa.rift");
		testSingleImportToPath(*imports[3], "/home/modules/", "/home/modules/a/b/c/d.rift");
		testSingleImportToPath(*imports[4], "/λ/", "/λ/ἄλφα/βῆτα/γάμμα.rift");
		testSingleImportToPath(*imports[5], "/🐋/💨", "/🐋/💨/ἄλφα/βῆτα/*.rift");
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/src/module_system/tests/");
