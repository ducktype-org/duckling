// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <backends/llvm/llvm_backend.hpp>
#include <driver/test_utils.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <global_state/backend_options.hpp>
#include <helios/queries/queries.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
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
		TESTER_ADD_TEST(staticArraysTest);
		TESTER_ADD_TEST(listsTest);
		TESTER_ADD_TEST(defaultInitialization);
		TESTER_ADD_TEST(classTest);
		TESTER_ADD_TEST(ffiTest);
		TESTER_ADD_TEST(tuplesTest);
		TESTER_ADD_TEST(pointersTest);
		TESTER_ADD_TEST(backendDependentTest);
	}

protected:
	void beforeAll() override {
		// Initialize the compiler so the standard library is loaded and select the LLVM backend.
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		auto         init_result    = compiler::driver::test_utils::initializeCompilerForTests(
            {},
            artifacts_path,
            { compiler::driver::options_types::StdLibOptions::DefaultStd{} },
            { .llvm_backend = global_state::BackendOptions::LLVMBackend{} }
        );
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");

		dia::configureImmediatePrint(&std::cerr);
	}

private:
	compiler::backend_llvm::Module getLLVMModuleFromPath(std::string module_path) {
		using namespace compiler;

		base::Optional<backend_llvm::Module> llvm_module_opt;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module
				= frontend::createModuleTreeWithRandomPackageID(fs::File(path(module_path)));
			auto& module_hout = ctx.query<helios::QueryModuleHOUT>(module)->valueOrPanic();

			auto mir_result = mir::lowerToMIRUnit(ctx, &module_hout).valueOrPanic();
			auto lir_result = lir::lowerToLIRUnit(ctx, mir_result);

			llvm_module_opt.emplace(
				backend_llvm::Module::fromLIRUnit(ctx, lir_result, base::StrID("test_module"))
			);
		});

		// debug print for coverage only:
		auto llvm_module_dprint = llvm_module_opt->dumpLLVMToString();

		// clone for coverage:
		auto cloned        = llvm_module_opt->clone();
		auto cloned_dprint = cloned.dumpLLVMToString();

		ASSERT_EQUAL(llvm_module_dprint, cloned_dprint);

		// verify integrity, then return for further checks.
		assertTrue(
			llvm_module_opt->verify().isOk(),
			"LLVM module verification failed (enable Backend dev logs to see details)"
		);

		assertTrue(cloned.verify().isOk(), "Cloned LLVM module verification failed");


		return std::move(llvm_module_opt.value());
	}

	void runTestForModule(
		std::string module_path, i32 expected_function_count = 1, i32 expected_prototype_count = -1
	) {
		if (expected_prototype_count == -1) expected_prototype_count = expected_function_count;
		auto llvm_module = getLLVMModuleFromPath(std::move(module_path));
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(false), expected_function_count);
		ASSERT_EQUAL_PRINT(llvm_module.getFunctionCount(), expected_prototype_count);
	}

	void returnVoidTest() { runTestForModule("modules/simple"); }

	void simpleTypesVariables() {
		// Currently this test is for coverage mainly, but it will be
		// replaced with something more meaningful in the future.

		runTestForModule("modules/variables");
	}

	void booleansTest() { runTestForModule("modules/booleans", 2, 3); }

	void arithmeticTest() { runTestForModule("modules/arithmetic"); }

	void comparisonTest() { runTestForModule("modules/comparison", 1, 3); }

	void functionCalls() {
		runTestForModule("modules/calls_simple", 3);
		runTestForModule("modules/calls", 3, 4);
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

	// Both globals are trivially destructible, so they only get a ctor each, and the module gets a
	// ctor calling them.
	void globalVariablesTest() { runTestForModule("modules/global-variables", 4, 5); }

	void unitsTest() {
		runTestForModule("modules/units/unit1", 2, 2);
		runTestForModule("modules/units/unit2", 2, 2);
		runTestForModule("modules/units/unit3", 1, 1);
		runTestForModule("modules/units/unit4", 1, 2);
		runTestForModule("modules/units/unit_simple", 2, 2);
		runTestForModule("modules/units/unit_class", 3, 3);
		runTestForModule("modules/units/unit_simple_multiple_modules", 1, 2);
	}

	void classTest() { runTestForModule("modules/classes/records", 7, 9); }

	void ffiTest() { runTestForModule("modules/ffi", 1, 1); }

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
		ASSERT_EQUAL_PRINT(ptr_loads, 17);
	}

	void boxesTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/boxes");
		std::string ir          = llvm_module.dumpLLVMToString();

		auto count_matches = [&](const std::string& text) {
			std::smatch matches;
			int         count        = 0;
			std::string search_range = ir;
			std::regex  ptr_load_regex{ text };
			while (std::regex_search(search_range, matches, ptr_load_regex)) {
				count++;
				search_range = matches.suffix();
			}
			return count;
		};

		// The storage of a box comes from `core.containers`: `new` calls the `boxAlloc` language
		// primitive, which on a native target ends up in libc `malloc`. Every step of that chain
		// is emitted into this module, so both ends are visible here.
		assertTrue(
			count_matches(R"(define linkonce_odr ptr @\S*8boxAlloc)") == 1,
			"Expected the baked 'boxAlloc' primitive to be emitted for 'box i32'"
		);
		assertTrue(
			count_matches(R"(define linkonce_odr void @\S*7boxFree)") == 1,
			"Expected the baked 'boxFree' primitive to be emitted for 'box i32'"
		);

		// `new 42` hands the boxed value to `boxAlloc`, which moves it into the fresh storage -
		// so the literal shows up as a call argument now, not as a store here.
		assertTrue(
			std::regex_search(ir, std::regex{ R"(call ptr @\S+\(i32 42\))" }),
			"Expected the boxed value 42 to be passed to the box allocation"
		);
		// `box i32` asks for 4 bytes, which the primitive reads off this baked constant.
		assertTrue(
			std::regex_search(
				ir, std::regex{ R"(@\S*LLVM_OBJECT_SIZE\S* = linkonce_odr constant i64 4)" }
			),
			"Expected the allocation size baked for 'box i32' to be 4"
		);

		int alloc_count   = count_matches(R"(call ptr @malloc\()");
		int dealloc_count = count_matches(R"(call void @free\(ptr)");
		ASSERT_EQUAL_PRINT(alloc_count, dealloc_count);
		ASSERT_EQUAL_PRINT(alloc_count, 1);
	}

	void staticArraysTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/static_arrays");
		std::string ir          = llvm_module.dumpLLVMToString();

		auto function_ir = [&](const std::string& name) -> std::string {
			std::smatch      header_match;
			const std::regex header_regex{ "define[^\\n]*" + name + "[^\\n]*\\{" };
			if (not std::regex_search(ir, header_match, header_regex)) {
				assertTrue(false, "Expected LLVM definition of " + name);
				return {};
			}

			const auto begin = static_cast<std::string::size_type>(header_match.position());
			const auto end   = ir.find("\n}", begin);
			assertTrue(end != std::string::npos, "Expected end of LLVM definition of " + name);
			return ir.substr(begin, end == std::string::npos ? 0 : end - begin + 2);
		};

		// Was [10 x i32] type found.
		assertTrue(
			std::regex_search(ir, std::regex{ R"(alloca\s+\[99000\s+x\s+i64\])" }),
			"Expected array type [99000 x i64]"
		);
		assertTrue(
			std::regex_search(
				ir,
				std::regex{
					R"(define[^\n]*large_array_test[\s\S]*?call\s+void\s+@llvm\.memset[^\n]*i8\s+0,\s+i64\s+792000,\s+i1\s+false)" }
			),
			"Expected zero-initialization of i64[99000] with memset"
		);
		assertFalse(
			std::regex_search(ir, std::regex{ R"(store\s+\[99000\s+x\s+i64\]\s+zeroinitializer)" }),
			"Large array must not be zero-initialized with an aggregate store"
		);

		// arr[3]
		assertTrue(
			std::regex_search(
				ir, std::regex{ R"(getelementptr.*\[11\s+x\s+i32\].*i32\s+0,\s+i64\s+%)" }
			),
			"Expected GEP instruction for array indexing arr[3]"
		);

		// Was [2 x [3 x i32]] type found.
		assertTrue(
			std::regex_search(ir, std::regex{ R"(\[2\s+x\s+\[3\s+x\s+i32\]\])" }),
			"Expected nested array type [2 x [3 x i32]]"
		);
		// Check if GEP with three indexes was generated.
		assertTrue(
			std::regex_search(
				ir,
				std::regex{
					R"(getelementptr.*\[2\s+x\s+\[3\s+x\s+i32\].*i32\s+0,\s+i64\s+%\w+,\s+i64\s+%\w+)" }
			),
			"Expected big GEP for nested array access matrix[1][2]"
		);

		// points[1].y
		// GEP: 0 (ptr), 1 (array index), 1 (field index)
		assertTrue(
			std::regex_search(
				ir, std::regex{ R"(getelementptr.*i32\s+0,\s+i64\s+%\w+,\s+i32\s+1)" }
			),
			"Expected GEP for struct field access in array: points[1].y"
		);

		const std::string large_copy_ir = function_ir("large_array_copy_test");
		assertTrue(
			std::regex_search(
				large_copy_ir,
				std::regex{ R"(call\s+void\s+@llvm\.memmove[^\n]*i64\s+792000,\s+i1\s+false)" }
			),
			"Expected the 99000-element array copy to use a 792000-byte memmove"
		);
		assertFalse(
			std::regex_search(large_copy_ir, std::regex{ R"((load|store)\s+\[99000\s+x\s+i64\])" }),
			"Large array copy must not use a whole-aggregate load or store"
		);

		const std::string large_sink_ir = function_ir("large_array_sink");
		assertTrue(
			std::regex_search(
				large_sink_ir,
				std::regex{
					R"(define[^\n]*large_array_sink[^\n]*\(ptr[^,\n]*byval\(\[99000\s+x\s+i64\]\))" }
			),
			"Expected a large array parameter to use ptr byval([99000 x i64])"
		);
		assertFalse(
			std::regex_search(large_sink_ir, std::regex{ R"(load\s+\[99000\s+x\s+i64\])" }),
			"A byval array parameter must not be loaded as one aggregate"
		);

		const std::string large_identity_ir = function_ir("large_array_identity");
		assertTrue(
			std::regex_search(
				large_identity_ir,
				std::regex{
					R"(define\s+void[^\n]*large_array_identity[^\n]*\(ptr[^,\n]*sret\(\[99000\s+x\s+i64\]\)[^,\n]*,\s*ptr[^,\n]*byval\(\[99000\s+x\s+i64\]\))" }
			),
			"Expected a hidden sret result followed by a byval array parameter"
		);
		assertTrue(
			std::regex_search(
				large_identity_ir,
				std::regex{ R"(call\s+void\s+@llvm\.memmove[^\n]*i64\s+792000,\s+i1\s+false)" }
			),
			"Expected a large returned array to be copied into sret storage"
		);
		assertFalse(
			std::regex_search(
				large_identity_ir, std::regex{ R"((load|store)\s+\[99000\s+x\s+i64\])" }
			),
			"A large returned array must stay in memory"
		);

		const std::string large_forward_ir = function_ir("large_array_forward");
		assertTrue(
			std::regex_search(
				large_forward_ir,
				std::regex{
					R"(define\s+void[^\n]*large_array_forward[^\n]*\(\s*ptr[^,\n]*sret\(\[99000\s+x\s+i64\]\)[^,%\n]*(%[-A-Za-z0-9$._]+),[^\n]*\)[^{]*\{[\s\S]*?call\s+void[^\n]*large_array_identity[^\n]*\(\s*ptr[^,\n]*sret\(\[99000\s+x\s+i64\]\)[^,%\n]*\1,\s*ptr[^,\n]*byval\(\[99000\s+x\s+i64\]\))" }
			),
			"Expected the nested call to receive the outer function's sret destination directly"
		);
		assertFalse(
			std::regex_search(large_forward_ir, std::regex{ R"(alloca\s+\[99000\s+x\s+i64\])" }),
			"Forwarding a large call result must not allocate an intermediate array"
		);
		assertFalse(
			std::regex_search(
				large_forward_ir, std::regex{ R"((load|store)\s+\[99000\s+x\s+i64\])" }
			),
			"A forwarded large result must not become an aggregate LLVM value"
		);
		assertFalse(
			std::regex_search(large_forward_ir, std::regex{ R"(@llvm\.(memcpy|memmove))" }),
			"Forwarding an sret destination must not copy the aggregate"
		);
	}

	void listsTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/lists");
		std::string ir          = llvm_module.dumpLLVMToString();

		assertTrue(
			std::regex_search(ir, std::regex{ R"(call\s+.*push)" }),
			"Expected a call to List's push method"
		);
	}

	void defaultInitialization() {
		auto        llvm_module = getLLVMModuleFromPath("modules/default_init");
		std::string ir          = llvm_module.dumpLLVMToString();

		// i32 = 0
		assertTrue(
			std::regex_search(ir, std::regex{ R"(store\s+i32\s+0,\s+ptr\s+%\w+)" }),
			"Expected default initialization of i32 with 0"
		);

		// f64 = 0.0
		assertTrue(
			std::regex_search(ir, std::regex{ R"(store\s+double\s+0\.0+e\+00,\s+ptr\s+%\w+)" }),
			"Expected default initialization of f64 with 0.000000e+00"
		);

		// Point occupies 8 bytes.
		assertTrue(
			std::regex_search(
				ir,
				std::regex{ R"(call\s+void\s+@llvm\.memset[^\n]*i8\s+0,\s+i64\s+8,\s+i1\s+false)" }
			),
			"Expected default initialization of Point with memset"
		);

		// i32[5] occupies 20 bytes.
		assertTrue(
			std::regex_search(
				ir,
				std::regex{ R"(call\s+void\s+@llvm\.memset[^\n]*i8\s+0,\s+i64\s+20,\s+i1\s+false)" }
			),
			"Expected default initialization of i32[5] with memset"
		);
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

	void tuplesTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/tuples");
		std::string ir          = llvm_module.dumpLLVMToString();

		// Ckeck if tuple types are present
		assertTrue(
			std::regex_search(ir, std::regex{ R"(%T.*E)" }), "Expected tuple struct definition"
		);
	}

	void pointersTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/pointers");
		std::string ir          = llvm_module.dumpLLVMToString();
	}

	void backendDependentTest() {
		auto        llvm_module = getLLVMModuleFromPath("modules/backend_dependent");
		std::string ir          = llvm_module.dumpLLVMToString();

		// The LLVM backend must compile the `@native_only_impl` of `getValue` (returning 20)
		// and never the `@dvm_only_impl` one (returning 10).
		assertTrue(
			std::regex_search(ir, std::regex{ R"(ret i32 20)" }),
			"Expected native_only_impl 'ret i32 20' in the LLVM module"
		);
		assertTrue(
			not std::regex_search(ir, std::regex{ R"(ret i32 10)" }),
			"dvm_only_impl 'ret i32 10' must not be compiled into the LLVM module"
		);
	}
};


TESTER_COMMON_MAIN("/src/compiler/core/backends/llvm/tests/")
