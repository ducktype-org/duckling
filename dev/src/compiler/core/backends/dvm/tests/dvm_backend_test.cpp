#include <backends/dvm/dvm_backend.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <vm_tester_utils.hpp>

#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/serializer/serializer.hpp>

#include <utility>

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
		TESTER_ADD_TEST(unitsTest);
	}

protected:
	void testWithLIR(query::Context& ctx, CRef<compiler::lir::Function> lir_function);

private:
	auto getModuleFromPath(std::string module_path) {
		using namespace compiler;

		vm::code::CodeCollection code;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module
				= frontend::createModuleTreeWithRandomPackageID(fs::File(path(module_path)));
			auto& top_level = ctx.query<helios::QueryTopLevelEntities>(module)->valueOrPanic();

			backend_vm::DVMCodeBuilder m(ctx, false);

			for (auto& hout_glob: top_level.glob_data) {
				auto lir_glob = lir::LIRGlobal::fromHOUT(ctx, hout_glob);
				variant_match(hout_glob.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_func = &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_glob })
						                     ->valueOrThrow();
						auto lir_func = ctx.query<lir::LowerToLIRFunction>({ mir_func });
						m.insertLirGlobal(
							lir_glob,
							// @TODO: #929 add legit dtors when implemented
							lir_func,
							{}
						);
					}
					variant_case(helios::HOUTGlobalConst, cnst) {
						// @TODO: #1553 -- const ctors will probably be added here
						// @TODO: #1709 For now, global meta type constants are skipped and not
						// treated as failure for the code using compile time evaluated types to
						// compile.
						if (!cnst.value.has<tsh::SymbolType<>>()) {
							fail(base::strConcat(
								"We fail here, because constants don't work on DVM as "
								"expected, "
								"remove "
								"the fail after #1553. ",
								"Global constant: ",
								hout_glob.original_name.strView()
							));
						}
					}
					variant_default {
						fail(base::strConcat(
							"Unexpected global data type in module: ",
							hout_glob.original_name.strView()
						));
					}
				}
			}
			for (auto& fun: top_level.functions) {
				auto mir_fun = ctx.query<compiler::mir::LowerToMIRFunction>({ fun });
				auto lir_fun = ctx.query<compiler::lir::LowerToLIRFunction>(
					{ &mir_fun->valueOrPanicMsg("Couldn\'t compile") }
				);
				m.insertLirFunction(lir_fun);
			}
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

	void simpleTest() { runTest("modules/simple", {}, {}, {}, 42); }

	void functionCallsTest() { runTest("modules/function_calls", {}, {}, {}, 4); }

	void builtinFuncsTest() { runTest("modules/builtin_funcs", "9", "81\n82\n", {}, 82); }

	void globalVariablesTest() {
		runTest(
			"modules/globals", {}, "10\n42\n99\n99\n42\n99\n43\n-42\n-41\n41\n777\n1\n0\n", {}, 0
		);
	}

	void booleanOperationsTest() { runTest("modules/boolean_operations", {}, {}, {}, 1); }

	void comparisonsTest() { runTest("modules/comparisons", {}, {}, {}, 55); }

	void referencesTest() {
		runTest(
			"modules/references",
			{},
			"10\n20\n20\n20\n20\n21\n16\n20\n-20\n-20\n-40\n-"
			"30\n222\n111\n222\n400\n400\n400\n500\n",
			{},
			0
		);
	}

	void unitsTest() { runTest("modules/units", {}, {}, {}, 0); }
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/dvm/tests/")
