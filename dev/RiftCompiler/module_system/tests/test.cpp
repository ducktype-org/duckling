#include <tester/tester.hpp>
#include <module_system/import_to_path.hpp>
#include <pst_parser/pst.hpp>
#include <lexer/lexer.hpp>

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
	pst::PST prepare(const std::string& filename) { return { fs::FilePath(filename) }; }

	void testSingleImportToPath(
		const pst::Import& import, const std::string& local_path, const std::string& expected_output
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

TESTER_COMMON_MAIN("/RiftCompiler/module_system/tests/");
