#include <backends/llvm/llvm_backend.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/backend_options.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <regex>
#include <utility>

class LLVMBackendTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LLVMBackendTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		global_state::setters::setBackendOptions({
			.llvm_backend = { global_state::BackendOptions::LLVMBackend{} },
		});

		TESTER_ADD_TEST(returnVoidTest);
		TESTER_ADD_TEST(simpleTypesVariables);
		TESTER_ADD_TEST(booleansTest);
		TESTER_ADD_TEST(arithmeticTest);
		TESTER_ADD_TEST(floatingPointTest);
		TESTER_ADD_TEST(comparisonTest);
		TESTER_ADD_TEST(functionCalls);
		TESTER_ADD_TEST(castsLoweringTest);
		TESTER_ADD_TEST(parseFromIRCodeTest);
		TESTER_ADD_TEST(doesNotParseIncorrectIRCode);
		TESTER_ADD_TEST(globalVariablesTest);
		TESTER_ADD_TEST(unitsTest);
		TESTER_ADD_TEST(referencesTest);
		TESTER_ADD_TEST(boxesTest);
		TESTER_ADD_TEST(classTest);
		TESTER_ADD_TEST(stringsTest);
		TESTER_ADD_TEST(ffiTest);
	}

private:
	auto getLLVMModuleFromPath(std::string module_path) {
		using namespace compiler;

		backend_llvm::Module             llvm_module(base::StrID("test_module"));
		std::vector<CRef<lir::Function>> ctors;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module
				= frontend::createModuleTreeWithRandomPackageID(fs::File(path(module_path)));
			auto module_hout = ctx.query<helios::QueryModuleHOUT>(module).valueOrPanic();

			for (auto& hout_glob: module_hout.glob_data) {
				if (!hout_glob.type.getType().carriesInformation(ctx)) continue;
				lir::LIRGlobal lir_glob = lir::LIRGlobal::fromHOUT(ctx, hout_glob);
				llvm_module.addGlobalToModule(lir_glob);
				variant_match(hout_glob.value) {
					variant_case(helios::HOUTGlobalVariable, var) {
						CRef mir_func = &ctx.query<mir::LowerGlobalDataToMIRCtor>({ hout_glob })
						                     ->valueOrThrow();
						mir_func->debugPrint(std::cerr);
						std::cerr << "\n\n\n";
						auto lir_func = ctx.query<lir::LowerToLIRFunction>({ mir_func });
						lir_func->debugPrint(ctx, std::cerr);
						std::cerr << "\n\n\n";
						ctors.push_back(lir_func);
						llvm_module.addFunctionToModule(ctx, lir_func);
					}
					variant_case(helios::HOUTGlobalConst, cnst) {
						// @future #1554 -- const ctors will probably be added here
					}
					variant_default {
						fail(base::strConcat(
							"Unexpected global data type of: ", hout_glob.original_name
						));
					}
				}
			}

			if (!ctors.empty()) {
				// Add module ctors
				auto module_ctor = lir::createFunctionInvoker(
					ctx,
					ctors,
					compiler::helios::mangler::getSpecialMangledName<
						compiler::helios::mangler::ManglingSymbolKind::ModuleConstructor>(
						ctx,
						compiler::helios::mangler::special_symbol_keys::LIRModuleID{
							frontend::moduleName(module) }
					)
				);
				llvm_module.addFunctionToModuleCtors(ctx, CRef<lir::Function>(&module_ctor));

				// Add module dtors (for now empty)
				// @TODO: add a legit dtors
				auto module_dtor = lir::createFunctionInvoker(
					ctx,
					{},
					compiler::helios::mangler::getSpecialMangledName<
						compiler::helios::mangler::ManglingSymbolKind::ModuleDestructor>(
						ctx,
						compiler::helios::mangler::special_symbol_keys::LIRModuleID{
							frontend::moduleName(module) }
					)
				);
				llvm_module.addFunctionToModuleDtors(ctx, CRef<lir::Function>(&module_dtor));
			}

			for (auto& fun: module_hout.functions) {
				CRef mir_fun
					= &ctx.query<compiler::mir::LowerToMIRFunction>({ fun })->valueOrThrow();
				auto lir_fun = ctx.query<compiler::lir::LowerToLIRFunction>({ mir_fun });
				llvm_module.addFunctionToModule(ctx, lir_fun);
			}
		});

		// debug print for coverage only:
		llvm_module.debugPrint();

		// verify integrity, then return for further checks.
		assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
		return llvm_module;
	}

	void runTestForModule(
		std::string module_path, i32 expected_function_count = 1, i32 expected_prototype_count = -1
	) {
		if (expected_prototype_count == -1) expected_prototype_count = expected_function_count;
		auto llvm_module = getLLVMModuleFromPath(std::move(module_path));
		llvm_module.debugPrint();
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(false), expected_function_count);
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(), expected_prototype_count);
	}

	void returnVoidTest() { runTestForModule("modules/simple"); }

	void simpleTypesVariables() {
		// Currently this test is for coverage mainly, but it will be
		// replaced with something more meaningful in the future.

		runTestForModule("modules/variables");
	}

	void booleansTest() { runTestForModule("modules/booleans", 2); }

	void arithmeticTest() { runTestForModule("modules/arithmetic"); }

	void comparisonTest() { runTestForModule("modules/comparison", 1, 2); }

	void functionCalls() {
		runTestForModule("modules/calls_simple", 3);
		runTestForModule("modules/calls", 3, 5);
	}

	void parseFromIRCodeTest() {
		auto llvm_module = compiler::backend_llvm::Module::fromIRCode(
			"define void @test() {\n"
			"entry:\n"
			"  ret void\n"
			"}\n"
		);
		llvm_module.debugPrint();

		assertTrue(llvm_module.verify().isOk(), "LLVM module verification failed");
	}

	void doesNotParseIncorrectIRCode() {
		assertThrows<base::Panic>(
			[&]() {
				auto llvm_module = compiler::backend_llvm::Module::fromIRCode(
					"define void @test() {\n"
					"entry:\n"
					"  re void\n"
					"}\n"
				);
			},
			"LLVM incorrect code didn't throw"
		);
	}

	void globalVariablesTest() { runTestForModule("modules/global-variables", 5, 5); }

	void unitsTest() {
		runTestForModule("modules/units/unit1", 2, 2);
		runTestForModule("modules/units/unit2", 2, 2);
		runTestForModule("modules/units/unit3", 1, 1);
		runTestForModule("modules/units/unit4", 1, 2);
		runTestForModule("modules/units/unit_simple", 2, 3);
		runTestForModule("modules/units/unit_class", 3, 4);
		runTestForModule("modules/units/unit_simple_multiple_modules", 1, 2);
	}

	void classTest() { runTestForModule("modules/classes/records", 8, 9); }

	void stringsTest() { runTestForModule("modules/strings", 1, 3); }

	void ffiTest() { runTestForModule("modules/ffi", 1, 2); }

	void referencesTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/references");
		std::string ir          = llvm_module.dumpLLVMToString();

		// Just a simple load count verification.
		std::smatch matches;
		int         ptr_loads    = 0;
		std::string search_range = ir;
		std::regex  ptr_load_regex{ R"(load ptr, ptr %\S+)" };
		while (std::regex_search(search_range, matches, ptr_load_regex)) {
			ptr_loads++;
			search_range = matches.suffix();
		}
		assertTrue(ptr_loads == 17, "Too few pointer loads");
	}

	void boxesTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/boxes");
		std::string ir          = llvm_module.dumpLLVMToString();

		std::cout << "LLVM IR:\n";
		std::cout << ir << '\n';
	}

	void floatingPointTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/floating_point");
		std::string ir          = llvm_module.dumpLLVMToString();

		assertTrue(
			std::regex_search(ir, std::regex{ R"(fadd\s+double)" }),
			"Expected 'fadd double' for f64 addition"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fsub\s+double)" }),
			"Expected 'fsub double' for f64 subtraction"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fmul\s+double)" }),
			"Expected 'fmul double' for f64 multiplication"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fdiv\s+double)" }),
			"Expected 'fdiv double' for f64 division"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fneg\s+double)" }),
			"Expected 'fneg double' for f64 negation"
		);

		assertTrue(
			std::regex_search(ir, std::regex{ R"(fadd\s+float)" }),
			"Expected 'fadd float' for f32 addition"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fsub\s+float)" }),
			"Expected 'fsub float' for f32 subtraction"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fmul\s+float)" }),
			"Expected 'fmul float' for f32 multiplication"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fdiv\s+float)" }),
			"Expected 'fdiv float' for f32 division"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fneg\s+float)" }),
			"Expected 'fneg float' for f32 negation"
		);

		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+olt\s+double)" }),
			"Expected 'fcmp olt' for f64 <"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+ogt\s+double)" }),
			"Expected 'fcmp ogt' for f64 >"
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+ole\s+double)" }),
			"Expected 'fcmp ole' for f64 <="
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+oge\s+double)" }),
			"Expected 'fcmp oge' for f64 >="
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+oeq\s+double)" }),
			"Expected 'fcmp oeq' for f64 =="
		);
		assertTrue(
			std::regex_search(ir, std::regex{ R"(fcmp\s+one\s+double)" }),
			"Expected 'fcmp one' for f64 !="
		);
	}

	void castsLoweringTest() {
		// Load module with cast test functions
		auto llvm_module = getLLVMModuleFromPath("modules/casts");

		std::string ir = llvm_module.dumpLLVMToString();

		bool has_i64_to_f64
			= std::regex_search(ir, std::regex{ R"(sitofp\s+i64\s+%[^\s]+\s+to\s+double)" });
		assertTrue(has_i64_to_f64, "Expected sitofp i64->f64 in IR");

		bool has_f64_to_i32_sat = std::regex_search(
			ir, std::regex{ R"(call\s+i32\s+@llvm\.fptosi\.sat\.i32\.f64\(double %\S+\))" }
		);
		assertTrue(has_f64_to_i32_sat, "Expected call to llvm.fptosi.sat.i32.f64 in IR");

		bool has_i64_to_i32
			= std::regex_search(ir, std::regex{ R"(trunc\s+i64\s+%\S+\s+to\s+i32)" });
		assertTrue(has_i64_to_i32, "Expected trunc i64->i32 in IR");

		bool has_i32_to_i64
			= std::regex_search(ir, std::regex{ R"((sext|zext)\s+i32\s+%\S+\s+to\s+i64)" });
		assertTrue(has_i32_to_i64, "Expected sext/zext i32-> i64 in IR");

		bool has_i1_to_i32
			= std::regex_search(ir, std::regex{ R"((sext|zext)\s+i1\s+%\S+\s+to\s+i32)" });
		assertTrue(has_i1_to_i32, "Expected sext/zext i1-> i32 in IR");

		bool has_i32_to_i1 = std::regex_search(ir, std::regex{ R"(icmp\sne\si64)" });
		assertTrue(has_i32_to_i1, "Expected icmp ne i64 in IR");

		bool has_i16_to_i64
			= std::regex_search(ir, std::regex{ R"((sext|zext)\s+i16\s+%\S+\s+to\s+i64)" });
		assertTrue(has_i16_to_i64, "Expected sext/zext i16-> i64 in IR");
	}
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/llvm/tests/")
