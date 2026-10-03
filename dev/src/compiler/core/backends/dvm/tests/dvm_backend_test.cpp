#include <backends/dvm/dvm_backend.hpp>
#include <driver/test_utils.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <program_lowering_context.hpp>
#include <tsl/queries.hpp>
#include <vm_tester_utils.hpp>

#include <base/extend_cpp/vector_utils.hpp>

#include <os_utils/system_libraries.hpp>
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
		TESTER_ADD_TEST(recordsTest);
		TESTER_ADD_TEST(staticArrayTest);
		TESTER_ADD_TEST(stringSliceTest);
		TESTER_ADD_TEST(unitsTest);
		TESTER_ADD_TEST(initsDeinitsTest);
		TESTER_ADD_TEST(pointersTest);
		TESTER_ADD_TEST(backendDependentTest);
		TESTER_ADD_TEST(allocTest);
		TESTER_ADD_TEST(ffiTest);
		TESTER_ADD_TEST(bitwiseOperationsTest);
		TESTER_ADD_TEST(variantUnitAlternativeTest);
	}

protected:
	void         testWithLIR(query::Context& ctx, CRef<compiler::lir::Function> lir_function);
	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();

	/**
	 * Initialize the compiler to have the ability to import from the standard library in tests.
	 * Treats the `modules` directory as a single package with each test module as a submodule.
	 */
	void beforeAll() override {
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("modules/simple/")), "simple" },
			{ fs::FilePath(path("modules/boolean_operations/")), "boolean_operations" },
			{ fs::FilePath(path("modules/builtin_funcs/")), "builtin_funcs" },
			{ fs::FilePath(path("modules/comparisons/")), "comparisons" },
			{ fs::FilePath(path("modules/function_calls/")), "function_calls" },
			{ fs::FilePath(path("modules/globals/")), "globals" },
			{ fs::FilePath(path("modules/records/")), "records" },
			{ fs::FilePath(path("modules/references/")), "references" },
			{ fs::FilePath(path("modules/static_arrays/")), "static_arrays" },
			{ fs::FilePath(path("modules/strings/")), "strings" },
			{ fs::FilePath(path("modules/units/")), "units" },
			{ fs::FilePath(path("modules/inits_deinits/")), "inits_deinits" },
			{ fs::FilePath(path("modules/pointers/")), "pointers" },
			{ fs::FilePath(path("modules/backend_dependent/")), "backend_dependent" },
			{ fs::FilePath(path("modules/alloc/")), "alloc" },
			{ fs::FilePath(path("modules/ffi/")), "ffi" },
			{ fs::FilePath(path("modules/bitwise_operations/")), "bitwise_operations" },
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	static inline const std::vector<std::string> ALL_CORE_MODULES{
		"core/builtins", "core/primitive_io", "core/containers",
		"core/runtime",  "core/panicking",    "core/clib",
	};

	auto getModuleFromPath(
		const std::string&              main_module_path,
		const std::vector<std::string>& module_paths_to_load = {}
	) {
		using namespace compiler;

		vm::code::CodeCollection code;

		auto append_module_to_code = [&](const std::string& module_path) {
			auto module = driver::test_utils::getModuleIdFromPath(module_path);
			query::utils::withContextDo([&](query::Context& ctx) {
				auto& top_level = ctx.query<helios::QueryModuleHOUT>(module)->valueOrPanic();

				auto mir_unit = mir::lowerToMIRUnit(ctx, &top_level);
				ASSERT_HAS_VALUE(mir_unit, "MIR lowering failed");

				auto lir_unit = lir::lowerToLIRUnit(ctx, mir_unit.valueOrPanic());

				backend_vm::DVMCodeBuilder m(ctx, base::StrID(module_path), false, false);
				m.insertLIRUnit(lir_unit);

				code.mergeFrom(m.build());
			});
		};
		for (auto& module_path: module_paths_to_load) append_module_to_code(module_path);
		append_module_to_code(main_module_path);

		base::deduplicateBy(code.functions, [](const vm::code::Function& f) { return f.name.str; });
		base::deduplicateBy(code.types, [](const vm::code::TypeOfData& f) { return typeName(f); });
		return code;
	}

	void runMultimoduleTest(
		const std::string&                 module_path,
		const std::vector<std::string>&    module_paths_to_load,
		const base::Optional<std::string>& input     = {},
		const base::Optional<std::string>& output    = {},
		const std::vector<std::string>&    args      = {},
		i64                                exit_code = 0
	) {
		using namespace compiler;
		auto code = getModuleFromPath(module_path, module_paths_to_load);
		// for (auto& type: code.types) vm::code::serializeType(type, std::cerr);
		// for (auto& func: code.functions) vm::code::serializeFunction(func, std::cerr);
		runTestOnVm(code, input, output, args, exit_code);
	}

	/**
	 * @brief Runs a module whose `extern("C")` calls are resolved from the system libraries.
	 * The compiler emits the `ffi function` declarations, but the shared objects to resolve them
	 * from come from the driver (`--dvm-shared-libs`), which is not part of this test, so they
	 * are declared here directly on the collection.
	 */
	void runFFITest(
		const std::string&                 module_path,
		const base::Optional<std::string>& output    = {},
		i64                                exit_code = 0
	) {
		auto code         = getModuleFromPath(module_path, ALL_CORE_MODULES);
		code.object_files = { os_utils::systemSharedLibC(), os_utils::systemSharedLibM() };
		runTestOnVm(code, {}, output, {}, exit_code);
	}

	void runTest(
		const std::string&                 module_path,
		const base::Optional<std::string>& input     = {},
		const base::Optional<std::string>& output    = {},
		const std::vector<std::string>&    args      = {},
		i64                                exit_code = 0
	) {
		runMultimoduleTest(module_path, {}, input, output, args, exit_code);
	}

	void runFailTest(
		const std::string&                 module_path,
		const std::string&                 fail_msg             = "",
		const base::Optional<std::string>& input                = {},
		const base::Optional<std::string>& output               = {},
		const std::vector<std::string>&    args                 = {},
		const std::vector<std::string>&    module_paths_to_load = {}
	) {
		using namespace compiler;
		auto code = getModuleFromPath(module_path, module_paths_to_load);
		// for (auto& type: code.types) vm::code::serializeType(type, std::cerr);
		// for (auto& func: code.functions) vm::code::serializeFunction(func, std::cerr);
		auto result = runTestOnVmGetResult(code, input, output, args);
		ASSERT_NO_VALUE(result.run_result);
		auto err_str = to_string(nlohmann::json(result.run_result.error()));
		if (not err_str.contains(fail_msg)) {
			fail(base::strConcat(
				"Expected error message to contain: \"", fail_msg, "\", but got: ", err_str
			));
		}

		ASSERT_HAS_VALUE(vm::api::kill(result.pid));
	}

	/**
	 * @brief A variant alternative carrying no information (`()`) has no DVM type of its own, but
	 * the DVM names alternatives by type name, so the lowering must name it with the stand-in
	 * `unit` opaque type instead of dropping it.
	 */
	void variantUnitAlternativeTest() {
		using namespace compiler;

		query::utils::withContextDo([&](query::Context& ctx) {
			const auto unit_type = tsh::SymbolType<>::withDefaults(tsh::getUnitType());
			const auto i64_type  = tsh::SymbolType<>::withDefaults(
                tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Signed)
            );
			const tsh::VariantAbstractType variant_type
				= ctx.query<tsh::QueryVariantType>({ { unit_type, i64_type } });

			CRef<tsl::TypeLayout> layout
				= &ctx.query<tsl::QueryAbstractTypeLayout>(variant_type)->valueOrThrow();

			backend_vm::internal::ProgramLoweringContext program_ctx(
				ctx, base::StrID("variant_unit_alternative_test"), false, false
			);
			const auto dvm_type = program_ctx.lowerAndKeepTslType(layout);

			const auto dvm_variant = vm::code::getTypeKind<vm::code::VariantType>(*dvm_type);
			ASSERT_HAS_VALUE(dvm_variant);

			const auto& unit_dvm_type = *program_ctx.lowerAndKeepTslType(
				&ctx.query<tsl::QueryAbstractTypeLayout>(unit_type.getType())->valueOrThrow()
			);
			ASSERT_HAS_VALUE(vm::code::getTypeKind<vm::code::OpaqueType>(unit_dvm_type));
			ASSERT_TRUE(std::ranges::contains(
				dvm_variant.value().variant_alternatives, typeName(unit_dvm_type)
			));
		});
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
		runMultimoduleTest(
			"static_arrays", ALL_CORE_MODULES, {}, "1\n100\n200\n300\n600\n20\n42\n11\n13\n4\n", {}, 0
		);
	}

	// A string literal is lowered to a static byte-array global plus a `{ptr, len}` slice struct.
	// Reading the length and indexing into the slice exercises the generated slice bytecode.
	void stringSliceTest() {
		runMultimoduleTest("strings", ALL_CORE_MODULES, {}, "14\nhello from vm!", {}, 0);
	}

	void unitsTest() { runTest("units", {}, {}, {}, 0); }

	// The pointer casts (including the `manyptr T` -> `ptr T` narrowing) run first and print their
	// results; the module then dereferences a null many-pointer, which must fail the process.
	void pointersTest() {
		runFailTest(
			"pointers", "Accessing null pointer", {}, "11\n44\n22\n0\n0\n1\n", {}, ALL_CORE_MODULES
		);
	}

	void initsDeinitsTest() { runTest("inits_deinits", {}, { "100\n" }, {}, 0); }

	// The DVM backend must select the `@dvm_only_impl` of the `@backend_dependent`
	// `getValue` (returning 10), not the `@native_only_impl` one (returning 20).
	void backendDependentTest() { runTest("backend_dependent", {}, {}, {}, 10); }

	void allocTest() {
		runMultimoduleTest("alloc", ALL_CORE_MODULES, {}, "16\n131\n145\n10\n", {}, 42);
	}

	// Calls into libc/libm through libffi: scalars, a struct returned by value, `cptr char`
	// strings, and `cptr`s to a struct, to a field, and to a static array element - both
	// projected by the DVM itself and written through by C.
	void ffiTest() {
		runFFITest(
			"ffi",
			"8\n0\n"
			"7\n33\n9\n33\n9\n4\n21\n21\n15\n33\n"
			"100\n2\n50\n0\n0\n3\n3\n"
			"5\n6\n7\n"
			"12\n13\n17\n8\n"
			"4\n50\n4\n"
		);
	}

	void bitwiseOperationsTest() { runTest("bitwise_operations", {}, {}, {}, 0); }
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/dvm/tests/")
