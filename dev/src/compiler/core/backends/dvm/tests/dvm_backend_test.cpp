#include <backends/dvm/dvm_backend.hpp>
#include <driver/initialize.hpp>
#include <driver/manifest/manifest.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/packages/packages.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <vm_tester_utils.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <utility>

using namespace compiler::driver;

class DVMBackendTest final: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DVMBackendTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(functionCallsTest);
		TESTER_ADD_TEST(builtinFuncsTest);
		TESTER_ADD_TEST(globalVariablesTest);
		TESTER_ADD_TEST(booleanOperationsTest);
		TESTER_ADD_TEST(comparisonsTest);
		TESTER_ADD_TEST(referencesTest);
		// @TODO: #2246 This test works well when compiled with `duckc dvm_run` although fails when
		// tested here because constructors aren't inserted properly. When this pipeline is unified,
		// uncomment this test.
		// TESTER_ADD_TEST(recordsTest);
		TESTER_ADD_TEST(staticArrayTest);
		TESTER_ADD_TEST(unitsTest);
		TESTER_ADD_TEST(initsDeinitsTest);
		TESTER_ADD_TEST(pointersTest);
	}

protected:
	void         testWithLIR(query::Context& ctx, CRef<compiler::lir::Function> lir_function);
	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();

	/**
	 * Initialize the compiler to have the ability to import from the standard library in tests.
	 * Treats the `modules` directory as a single package with each test module as a submodule.
	 */
	void beforeAll() override {
		global_state::setters::setGlobalLogger(makeBox<dia_int::Logger>());
		auto subpath_package = [&](const std::string& subpath) {
			return compiler::frontend::packages::RawPackageInfo{
				.package_name = base::StrID(subpath),
				.version      = base::StrID("0.1.0"),
				.package_path = fs::FilePath(path("modules/" + subpath + "/")),
				.features     = {},
				.dependencies = {},
			};
		};
		std::vector<compiler::frontend::packages::RawPackageInfo> packages{
			subpath_package("simple"),         subpath_package("boolean_operations"),
			subpath_package("builtin_funcs"),  subpath_package("comparisons"),
			subpath_package("function_calls"), subpath_package("globals"),
			subpath_package("records"),        subpath_package("references"),
			subpath_package("static_arrays"),  subpath_package("units"),
			subpath_package("inits_deinits"),  subpath_package("pointers")
		};
		auto init_result = compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = std::move(packages),
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.backend_options = {
					.llvm_backend = {},
				},
				.debug_options         = {},
				.incremental           = {},
				.execution_options     = { .worker_count = 1 },
				.stdlib_options = { .std_lib_type = compiler::driver::options_types::StdLibOptions::DefaultStd{} },
			}
		);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	/**
	 * Given module path from root module finds the submodule and returns its ID.
	 * It works like that because the whole `modules` directory is a single package root
	 * and each test module is a submodule of that package.
	 */
	compiler::frontend::ModuleID findSubmodule(
		compiler::frontend::ModuleID start_module, const std::vector<std::string>& path
	) {
		compiler::frontend::ModuleID current_module = start_module;
		for (const auto& part: path) {
			std::cerr << "Finding submodule: " << part << "\n";
			current_module = compiler::frontend::getModuleRef(current_module)
			                     ->getSubmoduleByName(base::StrID(part))
			                     .illegalAccess()
			                     ->illegalAccess()
			                     .getID();
		}
		return current_module;
	}

	auto getModuleFromPath(std::string module_path) {
		using namespace compiler;

		vm::code::CodeCollection code;
		std::vector<std::string> path_parts = module_path | std::views::split('/')
		                                    | std::views::transform([](auto&& part) {
												  return std::string(part.begin(), part.end());
											  })
		                                    | std::ranges::to<std::vector>();
		auto                     package_name = base::StrID(path_parts.front());
		std::vector<std::string> submodule_path_parts(path_parts.begin() + 1, path_parts.end());
		base::Optional<compiler::frontend::ModuleID> root_module_id;
		for (const auto& pkg_info: global_state::getPackages())
			if (pkg_info.getPackageID() == package_name)
				root_module_id = pkg_info.getRootModule().illegalAccess().getID();
		auto module = findSubmodule(*root_module_id, submodule_path_parts);

		query::utils::withContextDo([&](query::Context& ctx) {
			// @TODO: #2246 this duplicates the logic of compileLIRModuleToDVM, try to unify it
			// @TODO: #2246 remove query top level entities if possible

			auto& top_level = ctx.query<helios::QueryTopLevelEntities>(module)->valueOrPanic();

			auto mir_unit = mir::lowerToMIRUnit(ctx, &top_level);
			assertTrue(mir_unit.hasValue(), "MIR lowering failed");

			auto lir_unit = lir::lowerToLIRUnit(ctx, mir_unit.valueOrPanic());

			backend_vm::DVMCodeBuilder m(ctx, false, false);

			for (const auto& global: lir_unit.lir_globals) m.insertLirGlobal(global);

			for (const auto& lir_fun: lir_unit.lir_functions) m.insertLirFunction(lir_fun);
			code = m.build();
		});
		return code;
	}

	void runTest(
		std::string                        module_path,
		const base::Optional<std::string>& input     = {},
		const base::Optional<std::string>& output    = {},
		const std::vector<std::string>&    args      = {},
		i64                                exit_code = 0
	) {
		using namespace compiler;
		auto code = getModuleFromPath(std::move(module_path));
		for (auto& type: code.types) vm::code::serializeType(type, std::cerr);
		for (auto& func: code.functions) vm::code::serializeFunction(func, std::cerr);
		runTestOnVm(code, input, output, args, exit_code);
	}

	void runFailTest(
		std::string                        module_path,
		const std::string&                 fail_msg = "",
		const base::Optional<std::string>& input    = {},
		const base::Optional<std::string>& output   = {},
		const std::vector<std::string>&    args     = {}
	) {
		using namespace compiler;
		auto code = getModuleFromPath(std::move(module_path));
		for (auto& type: code.types) vm::code::serializeType(type, std::cerr);
		for (auto& func: code.functions) vm::code::serializeFunction(func, std::cerr);
		auto result = runTestOnVmGetResult(code, input, output, args);
		ASSERT_TRUE(not result.run_result.has_value());
		auto err_str = to_string(nlohmann::json(result.run_result.error()));
		if (not err_str.contains(fail_msg)) {
			fail(base::strConcat(
				"Expected error message to contain: \"", fail_msg, "\", but got: ", err_str
			));
		}
		const auto validation_result = vm::api::deinitAndValidate(result.pid);
		ASSERT_TRUE(validation_result.has_value());
	}

	void simpleTest() { runTest("simple", {}, {}, {}, 42); }

	void functionCallsTest() { runTest("function_calls", {}, {}, {}, 4); }

	void builtinFuncsTest() { runTest("builtin_funcs", "9", "81\n82\n", {}, 82); }

	void globalVariablesTest() {
		runTest("globals", {}, "10\n42\n99\n99\n42\n99\n43\n-42\n-41\n41\n777\n1\n0\n", {}, 0);
	}

	void booleanOperationsTest() { runTest("boolean_operations", {}, {}, {}, 1); }

	void comparisonsTest() { runTest("comparisons", {}, {}, {}, 55); }

	void referencesTest() {
		runTest(
			"references",
			{},
			"10\n20\n20\n20\n20\n21\n16\n20\n-20\n-20\n-40\n-"
			"30\n222\n111\n222\n400\n400\n400\n500\n",
			{},
			0
		);
	}

	void recordsTest() {
		runTest(
			"records", {}, "10\n20\n-1\n-2\n5\n15\n42\n50\n100\n101\n0\n300\n99\n2000\n0\n1\n", {}, 0
		);
	}

	void staticArrayTest() {
		runTest("static_arrays", {}, "1\n100\n200\n300\n600\n20\n42\n11\n13\n4\n", {}, 0);
	}

	void unitsTest() { runTest("units", {}, {}, {}, 0); }

	void pointersTest() { runFailTest("pointers", "Accessing null pointer", {}, {}, {}); }

	void initsDeinitsTest() { runTest("inits_deinits", {}, { "100\n" }, {}, 0); }
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/dvm/tests/")
