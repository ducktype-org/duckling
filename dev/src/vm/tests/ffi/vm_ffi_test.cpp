#include <vm_tester_utils.hpp>

#include <vm/api/vm.hpp>

#include <filesystem>
#include <fstream>

#ifndef FFI_TEST_LIB_PATH
	#error "FFI_TEST_LIB_PATH must be defined to the built shared object path"
#endif

namespace {
	// Absolute path to the shared object built alongside this test.
	const std::string SO_PATH = FFI_TEST_LIB_PATH;

	std::string ffiObjectHeader() { return "ffi object \"" + SO_PATH + "\";\n"; }

	// Bare soname of the system math library, resolved via the platform's dynamic loader search.
#ifdef __APPLE__
	const std::string SYSTEM_MATH_LIB = "libm.dylib";
#else
	const std::string SYSTEM_MATH_LIB = "libm.so.6";
#endif
}

class VmFfiTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFfiTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(addPrimitives);
		TESTER_ADD_TEST(voidAndStateReadback);
		TESTER_ADD_TEST(smallIntReturnWidening);
		TESTER_ADD_TEST(cptrRawReadWrite);
		TESTER_ADD_TEST(cptrStructField);
		TESTER_ADD_TEST(cptrCopyIntoStructField);
		TESTER_ADD_TEST(cptrNullTypedLoadFails);
		TESTER_ADD_TEST(cptrNullRawReadFails);
		TESTER_ADD_TEST(cptrNullCompare);
		TESTER_ADD_TEST(cptrTypedLoadStoreMallocWorkflow);
		TESTER_ADD_TEST(cptrAddOffsetArrayWalk);
		TESTER_ADD_TEST(cptrLoadThroughVoidCptrFails);
		TESTER_ADD_TEST(cptrLoadPointeeMismatchFails);
		TESTER_ADD_TEST(cptrLoadPackedPointeeFails);
		TESTER_ADD_TEST(cptrStoreThroughVoidCptrFails);
		TESTER_ADD_TEST(cptrStorePointeeMismatchFails);
		TESTER_ADD_TEST(cptrStorePackedPointeeFails);
		TESTER_ADD_TEST(cptrReadIntoPointerPointeeFails);
		TESTER_ADD_TEST(cptrWriteFromPointerPointeeFails);
		TESTER_ADD_TEST(cptrArrayReadWriteRoundTrip);
		TESTER_ADD_TEST(cptrArrayCountTooLargeFails);
		TESTER_ADD_TEST(cptrArrayNonTablePointeeFails);
		TESTER_ADD_TEST(cptrArrayPointerElementFails);
		TESTER_ADD_TEST(cptrAddOffsetAcrossTypesFails);
		TESTER_ADD_TEST(cptrCopyBuiltinsRemoved);
		TESTER_ADD_TEST(floatArgsAndReturn);
		TESTER_ADD_TEST(doubleArgsAndReturn);
		TESTER_ADD_TEST(mixedIntFloatArgs);
		TESTER_ADD_TEST(floatStructByValue);
		TESTER_ADD_TEST(floatNameWithWrongSizeFails);
		TESTER_ADD_TEST(systemLibraryViaApi);
		TESTER_ADD_TEST(bareSonameInBytecode);
		TESTER_ADD_TEST(missingSystemLibraryFails);
		TESTER_ADD_TEST(missingSymbolFails);
		TESTER_ADD_TEST(missingObjectFileFails);
		TESTER_ADD_TEST(unsupportedTypeFails);
		TESTER_ADD_TEST(multipleResultsFails);
		TESTER_ADD_TEST(alignedStructByValue);
		TESTER_ADD_TEST(structWithNestedTablesByValue);
		TESTER_ADD_TEST(structWithTableOfStructsByValue);
		TESTER_ADD_TEST(structWithFloatTableByValue);
		TESTER_ADD_TEST(structWithTableReturnedFromC);
		TESTER_ADD_TEST(nestedStructByValue);
		TESTER_ADD_TEST(tableByValueFails);
		TESTER_ADD_TEST(nonCompliantStructsFail);
		TESTER_ADD_TEST(structWithPackedFieldFails);
		TESTER_ADD_TEST(packedStructInFfiFails);
		TESTER_ADD_TEST(packedStructWithMatchingLayoutStillFails);
		TESTER_ADD_TEST(packedStructSize);
		TESTER_ADD_TEST(typedCPointerStructFieldThroughC);
		TESTER_ADD_TEST(forwardDeclaredPointee);
		TESTER_ADD_TEST(cpointerToUnknownTypeFails);
		TESTER_ADD_TEST(movPcptStrictnessFails);
		TESTER_ADD_TEST(duplicateFfiFunctionFails);
		TESTER_ADD_TEST(assertSizeMatches);
		TESTER_ADD_TEST(assertSizeMismatchFails);
		TESTER_ADD_TEST(assertSizeOnPointerTypeFails);
	}

private:
	// Writes bytecode text to a uniquely-named temporary file and returns it as an fs::File.
	fs::File writeTempDbc(const std::string& name, const std::string& contents) {
		auto          path = std::filesystem::temp_directory_path() / ("vm_ffi_" + name + ".dbc");
		std::ofstream out(path);
		out << contents;
		out.close();
		return { fs::FilePath(path.string()) };
	}

	// Loads a program expected to be valid and runs `main`, asserting the produced output.
	void runProgram(
		const std::string& name, const std::string& bytecode, const std::string& expected_output
	) {
		auto pid  = initProcess();
		auto file = writeTempDbc(name, bytecode);
		auto load = vm::api::loadFiles(pid, { file });
		if (!load.has_value()) fail(nlohmann::json(load.error()).dump());
		runTestOnVm(pid, {}, expected_output);
	}

	// Loads a program expected to fail validation, asserting the error mentions each keyword.
	void expectLoadError(
		const std::string&                   name,
		const std::string&                   bytecode,
		const std::vector<std::string_view>& keywords
	) {
		auto file = writeTempDbc(name, bytecode);
		auto load = vm::api::loadFiles(initProcess(), { file });
		ASSERT_NO_VALUE(load);
		ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(load.error()));
		auto why = std::get<vm::api::LoadProgramError>(load.error()).why;
		std::cerr << why << '\n';
		for (auto keyword: keywords)
			assertTrue(
				why.find(keyword) != std::string::npos, base::strConcat("Missing: ", keyword)
			);
	}

	void addPrimitives() {
		runProgram(
			"add",
			ffiObjectHeader()
				+ "ffi function ffi_add { i64, i64 } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type a, i64;\n"
				  "    init_pany_type b, i64;\n"
				  "    mov_p64_imm a, 20;\n"
				  "    mov_p64_imm b, 22;\n"
				  "    call_ffifunc ffi_add;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	void voidAndStateReadback() {
		runProgram(
			"void_state",
			ffiObjectHeader()
				+ "ffi function ffi_set { i64 } -> { };\n"
				  "ffi function ffi_get { } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 99;\n"
				  "    call_ffifunc ffi_set;\n"
				  "    init_pany_type g, i64;\n"
				  "    call_ffifunc ffi_get;\n"
				  "    output_p64 g;\n"
				  "    ret;\n"
				  "}\n",
			"99"
		);
	}

	void smallIntReturnWidening() {
		runProgram(
			"small",
			ffiObjectHeader()
				+ "ffi function ffi_small { } -> { i32 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type s, i32;\n"
				  "    call_ffifunc ffi_small;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"12345\n"
		);
	}

	// Raw byte copies through a `void*`-like cpointer, via the cptrWrite/cptrRead instructions.
	void cptrRawReadWrite() {
		runProgram(
			"cptr_raw_copy",
			ffiObjectHeader()
				+ "ffi function ffi_alloc8 { } -> { cptr };\n"
				  "ffi function ffi_free8 { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 555;\n"
				  "    init_pany_type vp, ptr_i64;\n"
				  "    ref_pptr_pany vp, v;\n"
				  "    cptrWrite_pcpt_pptr buf, vp;\n"
				  "    init_pany_type out, i64;\n"
				  "    init_pany_type op, ptr_i64;\n"
				  "    ref_pptr_pany op, out;\n"
				  "    cptrRead_pptr_pcpt op, buf;\n"
				  "    output_p64 out;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"555"
		);
	}

	// A structure with a `cptr` field passed to C by value.
	void cptrStructField() {
		runProgram(
			"cptr_struct",
			ffiObjectHeader()
				+ "type data: CPair { p: cptr, v: i64 } assert_size 16\n"
				  "ffi function ffi_alloc8 { } -> { cptr };\n"
				  "ffi function ffi_fill8 { cptr, i64 } -> { };\n"
				  "ffi function ffi_cpair_sum { CPair } -> { i64 };\n"
				  "ffi function ffi_free8 { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type buf2, cptr;\n"
				  "    mov_pcpt_pcpt buf2, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 30;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    init_pany_type bufc, cptr;\n"
				  "    mov_pcpt_pcpt bufc, buf;\n"
				  "    init_pany_type v2, i64;\n"
				  "    mov_p64_imm v2, 12;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type pair, CPair;\n"
				  "    structStore_pste_pany_field pair, bufc, CPair.p;\n"
				  "    structStore_pste_pany_field pair, v2, CPair.v;\n"
				  "    call_ffifunc ffi_cpair_sum;\n"
				  "    output_p64 res;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A mid-block pointer (here: a struct field) is a valid copy destination; the copy is
	// bounded by the field's own type.
	void cptrCopyIntoStructField() {
		runProgram(
			"copy_into_field",
			ffiObjectHeader()
				+ "type data: Pair { a: i64, b: i64 }\n"
				  "ffi function ffi_alloc8 { } -> { cptr };\n"
				  "ffi function ffi_fill8 { cptr, i64 } -> { };\n"
				  "ffi function ffi_free8 { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type buf2, cptr;\n"
				  "    mov_pcpt_pcpt buf2, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 4242;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    init_pany_type s, Pair;\n"
				  "    init_pany_type bp, ptr_i64;\n"
				  "    structLea_pptr_pste_field bp, s, Pair.b;\n"
				  "    cptrRead_pptr_pcpt bp, buf;\n"
				  "    init_pany_type out, i64;\n"
				  "    structLoad_pany_pste_field out, s, Pair.b;\n"
				  "    output_p64 out;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"4242"
		);
	}

	// A default-initialized cpointer is null; a typed load must fail cleanly, not crash.
	void cptrNullTypedLoadFails() {
		auto pid  = initProcess();
		auto file = writeTempDbc(
			"null_typed_load",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, I64Ptr;\n"
			"    init_pany_type v, i64;\n"
			"    cptrLoad_pany_pcpt v, p;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		if (!load.has_value()) fail(nlohmann::json(load.error()).dump());
		assertExecutionPanickedWith(runTestOnVmGetResult(pid), "Accessing null pointer");
	}

	// The same for the raw byte copy.
	void cptrNullRawReadFails() {
		auto pid  = initProcess();
		auto file = writeTempDbc(
			"null_raw_read",
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type out, i64;\n"
			"    init_pany_type op, ptr_i64;\n"
			"    ref_pptr_pany op, out;\n"
			"    cptrRead_pptr_pcpt op, c;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		if (!load.has_value()) fail(nlohmann::json(load.error()).dump());
		assertExecutionPanickedWith(runTestOnVmGetResult(pid), "Accessing null pointer");
	}

	// `cmpNull_pcpt` sets the flag on a null cpointer (default-initialized here) and clears it
	// on a live allocation; `cmov` materializes the flag as 1/0.
	void cptrNullCompare() {
		runProgram(
			"cptr_null_compare",
			ffiObjectHeader()
				+ "ffi function ffi_alloc8 { } -> { cptr };\n"
				  "ffi function ffi_free8 { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type nul, cptr;\n"
				  "    init_pany_type a, i64;\n"
				  "    mov_p64_imm a, 0;\n"
				  "    cmpNull_pcpt nul;\n"
				  "    cmov_p64_imm a, 1;\n"
				  "    output_p64 a;\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type b, i64;\n"
				  "    mov_p64_imm b, 0;\n"
				  "    cmpNull_pcpt buf;\n"
				  "    cmov_p64_imm b, 1;\n"
				  "    output_p64 b;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"10"
		);
	}

	void floatArgsAndReturn() {
		runProgram(
			"float_add",
			ffiObjectHeader()
				+ "type primitive: f32 4\n"
				  "ffi function ffi_addf { f32, f32 } -> { f32 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type res, f32;\n"
				  "    init_pany_type a, f32;\n"
				  "    init_pany_type b, f32;\n"
				  "    mov_p32_imm a, 20.25f32;\n"
				  "    mov_p32_imm b, 21.75f32;\n"
				  "    call_ffifunc ffi_addf;\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type ires, i32;\n"
				  "    fptosi_p32_p32 ires, res;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"42\n"
		);
	}

	void doubleArgsAndReturn() {
		runProgram(
			"double_add",
			ffiObjectHeader()
				+ "type primitive: f64 8\n"
				  "ffi function ffi_addd { f64, f64 } -> { f64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type res, f64;\n"
				  "    init_pany_type a, f64;\n"
				  "    init_pany_type b, f64;\n"
				  "    mov_p64_imm a, 20.25;\n"
				  "    mov_p64_imm b, 21.75;\n"
				  "    call_ffifunc ffi_addd;\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type ires, i32;\n"
				  "    fptosi_p32_p64 ires, res;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"42\n"
		);
	}

	void mixedIntFloatArgs() {
		runProgram(
			"mixed_args",
			ffiObjectHeader()
				+ "type primitive: f64 8\n"
				  "ffi function ffi_mix { i64, f64, i64, f64 } -> { f64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type res, f64;\n"
				  "    init_pany_type a, i64;\n"
				  "    init_pany_type b, f64;\n"
				  "    init_pany_type c, i64;\n"
				  "    init_pany_type d, f64;\n"
				  "    mov_p64_imm a, 40;\n"
				  "    mov_p64_imm b, 0.25;\n"
				  "    mov_p64_imm c, 1;\n"
				  "    mov_p64_imm d, 0.75;\n"
				  "    call_ffifunc ffi_mix;\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type ires, i32;\n"
				  "    fptosi_p32_p64 ires, res;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"42\n"
		);
	}

	void floatStructByValue() {
		runProgram(
			"float_struct",
			ffiObjectHeader()
				+ "type primitive: f32 4\n"
				  "type data: FPair { a: f32, b: f32 } assert_size 8\n"
				  "ffi function ffi_fpair_swap { FPair } -> { FPair };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type x, f32;\n"
				  "    init_pany_type res, FPair;\n"
				  "    init_pany_type p, FPair;\n"
				  "    mov_p32_imm x, 1.75f32;\n"
				  "    structStore_pste_pany_field p, x, FPair.a;\n"
				  "    mov_p32_imm x, 40.25f32;\n"
				  "    structStore_pste_pany_field p, x, FPair.b;\n"
				  "    call_ffifunc ffi_fpair_swap;\n"
				  "    init_pany_type sum, f32;\n"
				  "    structLoad_pany_pste_field sum, res, FPair.a;\n"
				  "    structLoad_pany_pste_field x, res, FPair.b;\n"
				  "    fadd_p32_p32 sum, x;\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type ires, i32;\n"
				  "    fptosi_p32_p32 ires, sum;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"42\n"
		);
	}

	void floatNameWithWrongSizeFails() {
		expectLoadError(
			"bad_float_size",
			ffiObjectHeader()
				+ "type primitive: f32 8\n"
				  "ffi function ffi_addf { f32, f32 } -> { f32 };\n",
			{ "FFI function signature" }
		);
	}

	// A system library injected through `loadCode` (the path the `--ffi-lib` CLI option uses)
	// must be visible to `ffi function` declarations from a later `loadFiles` call.
	void systemLibraryViaApi() {
		auto pid = initProcess();

		vm::code::CodeCollection libs;
		libs.object_files.emplace_back(SYSTEM_MATH_LIB);
		ASSERT_HAS_VALUE(vm::api::loadCode(pid, libs));

		auto file = writeTempDbc(
			"system_lib_api",
			"type primitive: f64 8\n"
			"ffi function cos { f64 } -> { f64 };\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type res, f64;\n"
			"    init_pany_type a, f64;\n"
			"    mov_p64_imm a, 0.0;\n"
			"    call_ffifunc cos;\n"
			"    init_pany_type r, i64;\n"
			"    init_pany_type ires, i32;\n"
			"    fptosi_p32_p64 ires, res;\n"
			"    call_builtinfunc builtin_output_i32;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		if (!load.has_value()) fail(nlohmann::json(load.error()).dump());
		runTestOnVm(pid, {}, "1\n");
	}

	// A bare soname in `ffi object` skips the file-existence check and is resolved by dlopen's
	// system library search.
	void bareSonameInBytecode() {
		runProgram(
			"system_lib_bytecode",
			"ffi object \"" + SYSTEM_MATH_LIB + "\";\n"
			"type primitive: f64 8\n"
			"ffi function cos { f64 } -> { f64 };\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type res, f64;\n"
			"    init_pany_type a, f64;\n"
			"    mov_p64_imm a, 0.0;\n"
			"    call_ffifunc cos;\n"
			"    init_pany_type r, i64;\n"
			"    init_pany_type ires, i32;\n"
			"    fptosi_p32_p64 ires, res;\n"
			"    call_builtinfunc builtin_output_i32;\n"
			"    ret;\n"
			"}\n",
			"1\n"
		);
	}

	void missingSystemLibraryFails() {
		expectLoadError(
			"missing_system_lib",
			"ffi object \"libduckling_no_such_lib.so\";\n"
			"ffi function ffi_add { i64, i64 } -> { i64 };\n",
			{ "Failed to load" }
		);
	}

	void missingSymbolFails() {
		expectLoadError(
			"missing_symbol",
			ffiObjectHeader() + "ffi function ffi_does_not_exist { } -> { };\n",
			{ "Symbol not found" }
		);
	}

	void missingObjectFileFails() {
		expectLoadError(
			"missing_object",
			"ffi object \"/definitely/not/a/real/library.so\";\n"
			"ffi function ffi_add { i64, i64 } -> { i64 };\n",
			{ "Failed to load" }
		);
	}

	void unsupportedTypeFails() {
		expectLoadError(
			"bad_type",
			ffiObjectHeader() + "ffi function ffi_add { string } -> { };\n",
			{ "FFI function signature" }
		);
	}

	void multipleResultsFails() {
		expectLoadError(
			"multi_result",
			ffiObjectHeader() + "ffi function ffi_add { i64, i64 } -> { i64, i64 };\n",
			{ "at most one" }
		);
	}

	// A non-packed struct follows the C layout rules (b is padded to offset 8), so it can be
	// passed by value even though its fields are not naturally packed.
	void alignedStructByValue() {
		runProgram(
			"aligned_struct",
			ffiObjectHeader()
				+ "type data: Mix { a: i8, b: i64 } assert_size 16\n"
				  "ffi function ffi_mix_sum { Mix } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type a, i8;\n"
				  "    init_pany_type b, i64;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type m, Mix;\n"
				  "    mov_p8_imm a, 2;\n"
				  "    mov_p64_imm b, 40;\n"
				  "    structStore_pste_pany_field m, a, Mix.a;\n"
				  "    structStore_pste_pany_field m, b, Mix.b;\n"
				  "    call_ffifunc ffi_mix_sum;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A fixed-size table field is flattened in the libffi descriptor (its element type repeated
	// once per element, recursively for a table of tables); the VM layout of `i32[2][2]` matches
	// C's `int32_t[4]`, so the struct passes as the C layout of an array member.
	void structWithNestedTablesByValue() {
		runProgram(
			"struct_with_nested_tables",
			ffiObjectHeader()
				+ "type fixed_size_table: arr2 i32 2\n"
				  "type fixed_size_table: arr2x2 arr2 2\n"
				  "type data: WithArr { v: arr2x2, tail: i64 } assert_size 24\n"
				  "ffi function ffi_arr_sum { WithArr } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type x, i32;\n"
				  "    init_pany_type i, i64;\n"
				  "    init_pany_type t, i64;\n"
				  "    init_pany_type row, arr2;\n"
				  "    init_pany_type m, arr2x2;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type w, WithArr;\n"
				  "    mov_p32_imm x, 1;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 row, x, i;\n"
				  "    mov_p32_imm x, 2;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 row, x, i;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 m, row, i;\n"
				  "    mov_p32_imm x, 3;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 row, x, i;\n"
				  "    mov_p32_imm x, 4;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 row, x, i;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 m, row, i;\n"
				  "    structStore_pste_pany_field w, m, WithArr.v;\n"
				  "    mov_p64_imm t, 32;\n"
				  "    structStore_pste_pany_field w, t, WithArr.tail;\n"
				  "    call_ffifunc ffi_arr_sum;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A fixed-size table of structs flattens to the struct type repeated per element, matching
	// the C layout of an array-of-structs member.
	void structWithTableOfStructsByValue() {
		runProgram(
			"struct_with_table_of_structs",
			ffiObjectHeader()
				+ "type data: Inner { x: i32, y: i32 } assert_size 8\n"
				  "type fixed_size_table: inner2 Inner 2\n"
				  "type data: PtTab { pts: inner2, tail: i64 } assert_size 24\n"
				  "ffi function ffi_pt_sum { PtTab } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type v, i32;\n"
				  "    init_pany_type p, Inner;\n"
				  "    init_pany_type i, i64;\n"
				  "    init_pany_type ps, inner2;\n"
				  "    init_pany_type t, i64;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type s, PtTab;\n"
				  "    mov_p32_imm v, 5;\n"
				  "    structStore_pste_pany_field p, v, Inner.x;\n"
				  "    mov_p32_imm v, 7;\n"
				  "    structStore_pste_pany_field p, v, Inner.y;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 ps, p, i;\n"
				  "    mov_p32_imm v, 9;\n"
				  "    structStore_pste_pany_field p, v, Inner.x;\n"
				  "    mov_p32_imm v, 11;\n"
				  "    structStore_pste_pany_field p, v, Inner.y;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 ps, p, i;\n"
				  "    structStore_pste_pany_field s, ps, PtTab.pts;\n"
				  "    mov_p64_imm t, 10;\n"
				  "    structStore_pste_pany_field s, t, PtTab.tail;\n"
				  "    call_ffifunc ffi_pt_sum;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A struct holding a table of `f32` is an all-SSE aggregate on SysV; the flattened elements
	// must map to `float`, not integers, for the correct classification.
	void structWithFloatTableByValue() {
		runProgram(
			"struct_with_float_table",
			ffiObjectHeader()
				+ "type primitive: f32 4\n"
				  "type fixed_size_table: farr4 f32 4\n"
				  "type data: FQuad { v: farr4 } assert_size 16\n"
				  "ffi function ffi_fquad_sum { FQuad } -> { f32 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type x, f32;\n"
				  "    init_pany_type i, i64;\n"
				  "    init_pany_type a, farr4;\n"
				  "    init_pany_type res, f32;\n"
				  "    init_pany_type q, FQuad;\n"
				  "    mov_p32_imm x, 10.5f32;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 a, x, i;\n"
				  "    mov_p32_imm x, 10.25f32;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 a, x, i;\n"
				  "    mov_p32_imm x, 10.75f32;\n"
				  "    mov_p64_imm i, 2;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 a, x, i;\n"
				  "    mov_p32_imm x, 10.5f32;\n"
				  "    mov_p64_imm i, 3;\n"
				  "    fixedSizeTableStore_pfst_pany_p64 a, x, i;\n"
				  "    structStore_pste_pany_field q, a, FQuad.v;\n"
				  "    call_ffifunc ffi_fquad_sum;\n"
				  "    init_pany_type r, i64;\n"
				  "    init_pany_type ires, i32;\n"
				  "    fptosi_p32_p32 ires, res;\n"
				  "    call_builtinfunc builtin_output_i32;\n"
				  "    ret;\n"
				  "}\n",
			"42\n"
		);
	}

	// A struct with a table field returned from C by value (the return path goes through a
	// hidden pointer for a 24-byte aggregate, unlike the argument path).
	void structWithTableReturnedFromC() {
		runProgram(
			"struct_with_table_returned",
			ffiObjectHeader()
				+ "type fixed_size_table: arr4 i32 4\n"
				  "type data: WithArr { v: arr4, tail: i64 } assert_size 24\n"
				  "ffi function ffi_arr_make { i32 } -> { WithArr };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type res, WithArr;\n"
				  "    init_pany_type base, i32;\n"
				  "    mov_p32_imm base, 6;\n"
				  "    call_ffifunc ffi_arr_make;\n"
				  "    init_pany_type a4, arr4;\n"
				  "    structLoad_pany_pste_field a4, res, WithArr.v;\n"
				  "    init_pany_type sum, i32;\n"
				  "    init_pany_type x, i32;\n"
				  "    init_pany_type i, i64;\n"
				  "    mov_p32_imm sum, 0;\n"
				  "    mov_p64_imm i, 0;\n"
				  "    fixedSizeTableLoad_pany_pfst_p64 x, a4, i;\n"
				  "    add_p32_p32 sum, x;\n"
				  "    mov_p64_imm i, 1;\n"
				  "    fixedSizeTableLoad_pany_pfst_p64 x, a4, i;\n"
				  "    add_p32_p32 sum, x;\n"
				  "    mov_p64_imm i, 2;\n"
				  "    fixedSizeTableLoad_pany_pfst_p64 x, a4, i;\n"
				  "    add_p32_p32 sum, x;\n"
				  "    mov_p64_imm i, 3;\n"
				  "    fixedSizeTableLoad_pany_pfst_p64 x, a4, i;\n"
				  "    add_p32_p32 sum, x;\n"
				  "    init_pany_type tail, i64;\n"
				  "    structLoad_pany_pste_field tail, res, WithArr.tail;\n"
				  "    init_pany_type total, i64;\n"
				  "    sext_p64_p32 total, sum;\n"
				  "    add_p64_p64 total, tail;\n"
				  "    output_p64 total;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A plain struct nested in another plain struct is FFI-compliant (compliance is recursive).
	void nestedStructByValue() {
		runProgram(
			"nested_struct",
			ffiObjectHeader()
				+ "type data: Inner { x: i32, y: i32 } assert_size 8\n"
				  "type data: Outer { first: Inner, z: i64 } assert_size 16\n"
				  "ffi function ffi_nested_sum { Outer } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type v, i32;\n"
				  "    init_pany_type z, i64;\n"
				  "    init_pany_type first, Inner;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type o, Outer;\n"
				  "    mov_p32_imm v, 20;\n"
				  "    structStore_pste_pany_field first, v, Inner.x;\n"
				  "    mov_p32_imm v, 15;\n"
				  "    structStore_pste_pany_field first, v, Inner.y;\n"
				  "    structStore_pste_pany_field o, first, Outer.first;\n"
				  "    mov_p64_imm z, 7;\n"
				  "    structStore_pste_pany_field o, z, Outer.z;\n"
				  "    call_ffifunc ffi_nested_sum;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// C has no by-value arrays, so a fixed-size table is only allowed as a structure field.
	void tableByValueFails() {
		expectLoadError(
			"table_by_value",
			ffiObjectHeader()
				+ "type fixed_size_table: arr4 i32 4\n"
				  "ffi function ffi_add { arr4 } -> { i64 };\n",
			{ "by-value arrays", "fixed-size table" }
		);
	}

	// Structures whose libffi descriptor would be degenerate are not FFI-compliant: a
	// zero-length table flattens to no elements, a field-less struct has size 0 (rejected by
	// libffi), and an oversized flattened element count must fail before it is materialized —
	// also when the elements hide inside nested structure descriptors.
	void nonCompliantStructsFail() {
		expectLoadError(
			"zero_length_table_struct",
			ffiObjectHeader()
				+ "type fixed_size_table: arr0 i32 0\n"
				  "type data: WithEmpty { v: arr0 }\n"
				  "ffi function ffi_add { WithEmpty } -> { i64 };\n",
			{ "WithEmpty", "cannot be used in an FFI function signature" }
		);
		expectLoadError(
			"empty_struct",
			ffiObjectHeader()
				+ "type data: Empty { }\n"
				  "ffi function ffi_add { Empty } -> { i64 };\n",
			{ "Empty", "cannot be used in an FFI function signature" }
		);
		expectLoadError(
			"huge_table_struct",
			ffiObjectHeader()
				+ "type fixed_size_table: huge i32 100000000\n"
				  "type data: WithHuge { v: huge }\n"
				  "ffi function ffi_add { WithHuge } -> { i64 };\n",
			{ "WithHuge", "cannot be used in an FFI function signature" }
		);
		// Each nested struct is under the cap on its own; their sum is not.
		expectLoadError(
			"nested_struct_cap_bypass",
			ffiObjectHeader()
				+ "type fixed_size_table: big i32 40000\n"
				  "type data: Inner { v: big }\n"
				  "type data: Outer { a: Inner, b: Inner }\n"
				  "ffi function ffi_add { Outer } -> { i64 };\n",
			{ "Outer", "cannot be used in an FFI function signature" }
		);
	}

	// A packed struct is not FFI-compliant, so neither is any struct containing one.
	void structWithPackedFieldFails() {
		expectLoadError(
			"packed_field_struct",
			ffiObjectHeader()
				+ "type data: PackedInner { a: i8, b: i64 } packed\n"
				  "type data: Holder { p: PackedInner, v: i64 }\n"
				  "ffi function ffi_add { Holder } -> { i64 };\n",
			{ "cannot be used in an FFI function signature", "Holder" }
		);
	}

	// libffi can only describe the C ABI layout, so packed structs are rejected in FFI
	// signatures.
	void packedStructInFfiFails() {
		expectLoadError(
			"packed_struct_ffi",
			ffiObjectHeader()
				+ "type data: Mix { a: i8, b: i64 } packed\n"
				  "ffi function ffi_mix_sum { Mix } -> { i64 };\n",
			{ "packed", "cannot be used in an FFI function signature" }
		);
	}

	// Rejected even when the packed layout coincides with the C ABI layout (all fields naturally
	// aligned) - such a struct is identical to its non-packed version, so `packed` is dropped
	// rather than special-cased.
	void packedStructWithMatchingLayoutStillFails() {
		expectLoadError(
			"packed_struct_matching_ffi",
			ffiObjectHeader()
				+ "type primitive: f32 4\n"
				  "type data: FPair { a: f32, b: f32 } packed assert_size 8\n"
				  "ffi function ffi_fpair_swap { FPair } -> { FPair };\n",
			{ "packed", "cannot be used in an FFI function signature" }
		);
	}

	// `packed` restores the no-padding layout: 1 + 8 bytes.
	void packedStructSize() {
		auto file = writeTempDbc(
			"packed_size",
			"type data: Mix { a: i8, b: i64 } packed assert_size 9\n"
			"function main { i64, ptr_argv } -> { i64 } { ret; }\n"
		);
		ASSERT_HAS_VALUE(vm::api::loadFiles(initProcess(), { file }));
	}

	void assertSizeMatches() {
		auto file = writeTempDbc(
			"assert_ok",
			"type data: Pair { a: i32, b: i32 } assert_size 8\n"
			"function main { i64, ptr_argv } -> { i64 } { ret; }\n"
		);
		ASSERT_HAS_VALUE(vm::api::loadFiles(initProcess(), { file }));
	}

	void assertSizeMismatchFails() {
		expectLoadError(
			"assert_bad",
			"type data: Pair { a: i32, b: i32 } assert_size 16\n",
			{ "assert_size` mismatch" }
		);
	}

	// A type whose size depends on the pointer width (8 bytes in C, 16 in the safe interpreter)
	// has no single size to assert against.
	void assertSizeOnPointerTypeFails() {
		expectLoadError(
			"assert_ptr",
			"type data: Holder { p: ptr_i64 } assert_size 8\n",
			{ "depends on the pointer width" }
		);
	}

	// A typed cpointer crosses the FFI boundary like the builtin `cptr` (the pointee type only
	// exists on the VM side), both as a bare argument/result and as a by-value struct field; C
	// writes through the pointer.
	void typedCPointerStructFieldThroughC() {
		runProgram(
			"typed_cpointer_struct",
			ffiObjectHeader()
				+ "type cpointer: I64Ptr i64\n"
				  "type data: Tagged { p: I64Ptr, tag: i64 } assert_size 16\n"
				  "ffi function ffi_alloc8 { } -> { I64Ptr };\n"
				  "ffi function ffi_tagged_store { Tagged } -> { };\n"
				  "ffi function ffi_read8 { I64Ptr } -> { i64 };\n"
				  "ffi function ffi_free8 { I64Ptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, I64Ptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type bufc, I64Ptr;\n"
				  "    mov_pcpt_pcpt bufc, buf;\n"
				  "    init_pany_type tag, i64;\n"
				  "    mov_p64_imm tag, 42;\n"
				  "    init_pany_type t, Tagged;\n"
				  "    structStore_pste_pany_field t, bufc, Tagged.p;\n"
				  "    structStore_pste_pany_field t, tag, Tagged.tag;\n"
				  "    call_ffifunc ffi_tagged_store;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type buf2, I64Ptr;\n"
				  "    mov_pcpt_pcpt buf2, buf;\n"
				  "    call_ffifunc ffi_read8;\n"
				  "    output_p64 res;\n"
				  "    init_pany_type buf3, I64Ptr;\n"
				  "    mov_pcpt_pcpt buf3, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// A cpointer never recurses into its pointee, covering the C handle idiom (e.g. `FILE*`:
	// the pointee is an opaque type that is never inspected) and the linked-list idiom (a
	// structure containing a cpointer to itself, declared before the structure).
	void forwardDeclaredPointee() {
		runProgram(
			"forward_declared_pointee",
			ffiObjectHeader()
				+ "type opaque: Handle 8\n"
				  "type cpointer: HandlePtr Handle\n"
				  "type cpointer: NodePtr Node\n"
				  "type data: Node { next: NodePtr, v: i64 } assert_size 16\n"
				  "ffi function ffi_alloc8 { } -> { HandlePtr };\n"
				  "ffi function ffi_free8 { HandlePtr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type h, HandlePtr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type h2, HandlePtr;\n"
				  "    mov_pcpt_pcpt h2, h;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    init_pany_type ok, i64;\n"
				  "    mov_p64_imm ok, 1;\n"
				  "    output_p64 ok;\n"
				  "    ret;\n"
				  "}\n",
			"1"
		);
	}

	void cpointerToUnknownTypeFails() {
		expectLoadError(
			"cpointer_unknown_inner",
			"type cpointer: BadPtr NoSuchType\n",
			{ "subtype is not defined", "NoSuchType" }
		);
	}

	// `mov_pcpt_pcpt` requires identical types on both sides: no implicit pointee change, no
	// mixing the builtin `cptr` (unknown pointee) with a typed cpointer, distinct names stay
	// distinct even with the same pointee, and non-cpointer operands are rejected outright.
	void movPcptStrictnessFails() {
		auto expect_mov_error = [&](const std::string& name,
		                            const std::string& types,
		                            const std::string& a_type,
		                            const std::string& b_type,
		                            std::string_view   keyword) {
			expectLoadError(
				name,
				types + "function main { i64, ptr_argv } -> { i64 } {\n"
				      + "    init_pany_type a, " + a_type + ";\n"
				      + "    init_pany_type b, " + b_type + ";\n"
				      + "    mov_pcpt_pcpt a, b;\n"
				        "    ret;\n"
				        "}\n",
				{ keyword }
			);
		};
		expect_mov_error(
			"mov_pcpt_across_types",
			"type cpointer: APtr i64\ntype cpointer: BPtr i32\n",
			"APtr",
			"BPtr",
			"C pointer type does not match"
		);
		expect_mov_error(
			"mov_pcpt_cptr_to_typed",
			"type cpointer: APtr i64\n",
			"APtr",
			"cptr",
			"C pointer type does not match"
		);
		expect_mov_error(
			"mov_pcpt_same_pointee",
			"type cpointer: APtr i64\ntype cpointer: BPtr i64\n",
			"APtr",
			"BPtr",
			"C pointer type does not match"
		);
		expect_mov_error(
			"mov_pcpt_non_cpointer", "", "i64", "i64", "Invalid instruction argument type"
		);
	}

	// The malloc workflow: allocate raw C memory, cast the `void*` to a typed cpointer, then
	// move a whole struct across the boundary with the typed load/store instructions.
	void cptrTypedLoadStoreMallocWorkflow() {
		runProgram(
			"typed_load_store",
			ffiObjectHeader()
				+ "type data: Pair { a: i64, b: i64 } assert_size 16\n"
				  "type cpointer: PairPtr Pair\n"
				  "ffi function ffi_alloc { i64 } -> { cptr };\n"
				  "ffi function ffi_free { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    init_pany_type n, i64;\n"
				  "    mov_p64_imm n, 16;\n"
				  "    call_ffifunc ffi_alloc;\n"
				  "    init_pany_type pp, PairPtr;\n"
				  "    cptrCast_pcpt_pcpt pp, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    init_pany_type s, Pair;\n"
				  "    mov_p64_imm v, 30;\n"
				  "    structStore_pste_pany_field s, v, Pair.a;\n"
				  "    mov_p64_imm v, 12;\n"
				  "    structStore_pste_pany_field s, v, Pair.b;\n"
				  "    cptrStore_pcpt_pany pp, s;\n"
				  "    init_pany_type s2, Pair;\n"
				  "    cptrLoad_pany_pcpt s2, pp;\n"
				  "    init_pany_type x, i64;\n"
				  "    init_pany_type sum, i64;\n"
				  "    structLoad_pany_pste_field sum, s2, Pair.a;\n"
				  "    structLoad_pany_pste_field x, s2, Pair.b;\n"
				  "    add_p64_p64 sum, x;\n"
				  "    output_p64 sum;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// Walks a C array with byte-wise pointer arithmetic: store/load through offset cpointers.
	void cptrAddOffsetArrayWalk() {
		runProgram(
			"add_offset_walk",
			ffiObjectHeader()
				+ "type cpointer: I64Ptr i64\n"
				  "ffi function ffi_alloc { i64 } -> { cptr };\n"
				  "ffi function ffi_free { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    init_pany_type n, i64;\n"
				  "    mov_p64_imm n, 16;\n"
				  "    call_ffifunc ffi_alloc;\n"
				  "    init_pany_type p, I64Ptr;\n"
				  "    cptrCast_pcpt_pcpt p, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 40;\n"
				  "    cptrStore_pcpt_pany p, v;\n"
				  "    init_pany_type off, i64;\n"
				  "    mov_p64_imm off, 8;\n"
				  "    init_pany_type p2, I64Ptr;\n"
				  "    cptrAddOffset_pcpt_pcpt_p64 p2, p, off;\n"
				  "    mov_p64_imm v, 2;\n"
				  "    cptrStore_pcpt_pany p2, v;\n"
				  "    init_pany_type x0, i64;\n"
				  "    cptrLoad_pany_pcpt x0, p;\n"
				  "    init_pany_type x1, i64;\n"
				  "    cptrLoad_pany_pcpt x1, p2;\n"
				  "    add_p64_p64 x0, x1;\n"
				  "    output_p64 x0;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// The builtin `cptr` has no pointee, so it cannot be dereferenced with a typed load.
	void cptrLoadThroughVoidCptrFails() {
		expectLoadError(
			"load_through_void",
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type v, i64;\n"
			"    cptrLoad_pany_pcpt v, c;\n"
			"    ret;\n"
			"}\n",
			{ "cannot be dereferenced" }
		);
	}

	void cptrLoadPointeeMismatchFails() {
		expectLoadError(
			"load_pointee_mismatch",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, I64Ptr;\n"
			"    init_pany_type v, i32;\n"
			"    cptrLoad_pany_pcpt v, p;\n"
			"    ret;\n"
			"}\n",
			{ "does not match the C pointer's pointee" }
		);
	}

	// A packed pointee is not FFI-compliant, so its native layout cannot be assumed.
	void cptrLoadPackedPointeeFails() {
		expectLoadError(
			"load_packed_pointee",
			"type data: P { a: i8, b: i64 } packed\n"
			"type cpointer: PPtr P\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, PPtr;\n"
			"    init_pany_type s, P;\n"
			"    cptrLoad_pany_pcpt s, p;\n"
			"    ret;\n"
			"}\n",
			{ "cannot be dereferenced" }
		);
	}

	// The store-side mirror of cptrLoadThroughVoidCptrFails.
	void cptrStoreThroughVoidCptrFails() {
		expectLoadError(
			"store_through_void",
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type v, i64;\n"
			"    cptrStore_pcpt_pany c, v;\n"
			"    ret;\n"
			"}\n",
			{ "cannot be dereferenced" }
		);
	}

	// The store-side mirror of cptrLoadPointeeMismatchFails.
	void cptrStorePointeeMismatchFails() {
		expectLoadError(
			"store_pointee_mismatch",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, I64Ptr;\n"
			"    init_pany_type v, i32;\n"
			"    cptrStore_pcpt_pany p, v;\n"
			"    ret;\n"
			"}\n",
			{ "does not match the C pointer's pointee" }
		);
	}

	// The store-side mirror of cptrLoadPackedPointeeFails.
	void cptrStorePackedPointeeFails() {
		expectLoadError(
			"store_packed_pointee",
			"type data: P { a: i8, b: i64 } packed\n"
			"type cpointer: PPtr P\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, PPtr;\n"
			"    init_pany_type s, P;\n"
			"    cptrStore_pcpt_pany p, s;\n"
			"    ret;\n"
			"}\n",
			{ "cannot be dereferenced" }
		);
	}

	// A raw copy must not overwrite VM-managed data (here: a struct with a pointer field).
	void cptrReadIntoPointerPointeeFails() {
		expectLoadError(
			"read_into_pointer_pointee",
			"type data: Holder { p: ptr_i64, v: i64 }\n"
			"type pointer: HolderPtr Holder\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type hp, HolderPtr;\n"
			"    cptrRead_pptr_pcpt hp, c;\n"
			"    ret;\n"
			"}\n",
			{ "trivially copyable" }
		);
	}

	// A raw copy must not leak raw block addresses to native memory either.
	void cptrWriteFromPointerPointeeFails() {
		expectLoadError(
			"write_from_pointer_pointee",
			"type data: Holder { p: ptr_i64, v: i64 }\n"
			"type pointer: HolderPtr Holder\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type hp, HolderPtr;\n"
			"    cptrWrite_pcpt_pptr c, hp;\n"
			"    ret;\n"
			"}\n",
			{ "trivially copyable" }
		);
	}

	// A dynamic table round trip through native memory: write two elements out with
	// cptrWriteArray, read them back into a second table with cptrReadArray.
	void cptrArrayReadWriteRoundTrip() {
		runProgram(
			"cptr_array_roundtrip",
			ffiObjectHeader()
				+ "type dynamic_table: dyn_i64 i64\n"
				  "type pointer: pdyn dyn_i64\n"
				  "ffi function ffi_alloc { i64 } -> { cptr };\n"
				  "ffi function ffi_free { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type src, pdyn;\n"
				  "    init_pany_type n, i64;\n"
				  "    mov_p64_imm n, 2;\n"
				  "    dynTableReAlloc_pptr_type_p64 src, dyn_i64, n;\n"
				  "    init_pany_type v, i64;\n"
				  "    init_pany_type idx, i64;\n"
				  "    mov_p64_imm v, 40;\n"
				  "    mov_p64_imm idx, 0;\n"
				  "    dynTableStore_pptr_pany_p64 src, v, idx;\n"
				  "    mov_p64_imm v, 2;\n"
				  "    mov_p64_imm idx, 1;\n"
				  "    dynTableStore_pptr_pany_p64 src, v, idx;\n"
				  "    init_pany_type buf, cptr;\n"
				  "    init_pany_type bytes, i64;\n"
				  "    mov_p64_imm bytes, 16;\n"
				  "    call_ffifunc ffi_alloc;\n"
				  "    cptrWriteArray_pcpt_pptr_p64 buf, src, n;\n"
				  "    init_pany_type dst, pdyn;\n"
				  "    dynTableReAlloc_pptr_type_p64 dst, dyn_i64, n;\n"
				  "    cptrReadArray_pptr_pcpt_p64 dst, buf, n;\n"
				  "    init_pany_type x0, i64;\n"
				  "    mov_p64_imm idx, 0;\n"
				  "    dynTableLoad_pany_pptr_p64 x0, dst, idx;\n"
				  "    init_pany_type x1, i64;\n"
				  "    mov_p64_imm idx, 1;\n"
				  "    dynTableLoad_pany_pptr_p64 x1, dst, idx;\n"
				  "    add_p64_p64 x0, x1;\n"
				  "    output_p64 x0;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcpt_pcpt buf_f, buf;\n"
				  "    call_ffifunc ffi_free;\n"
				  "    free_pptr src;\n"
				  "    free_pptr dst;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// An element count larger than the table's current size must raise a runtime exception
	// instead of copying past the table.
	void cptrArrayCountTooLargeFails() {
		auto pid = initProcess();
		auto file = writeTempDbc(
			"cptr_array_too_large",
			ffiObjectHeader()
				+ "type dynamic_table: dyn_i64 i64\n"
				  "type pointer: pdyn dyn_i64\n"
				  "ffi function ffi_alloc { i64 } -> { cptr };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type src, pdyn;\n"
				  "    init_pany_type n, i64;\n"
				  "    mov_p64_imm n, 2;\n"
				  "    dynTableReAlloc_pptr_type_p64 src, dyn_i64, n;\n"
				  "    init_pany_type buf, cptr;\n"
				  "    init_pany_type bytes, i64;\n"
				  "    mov_p64_imm bytes, 24;\n"
				  "    call_ffifunc ffi_alloc;\n"
				  "    init_pany_type n2, i64;\n"
				  "    mov_p64_imm n2, 3;\n"
				  "    cptrWriteArray_pcpt_pptr_p64 buf, src, n2;\n"
				  "    ret;\n"
				  "}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		if (!load.has_value()) fail(nlohmann::json(load.error()).dump());
		assertExecutionPanickedWith(
			runTestOnVmGetResult(pid), "cptrWriteArray: element count exceeds the table size"
		);
	}

	// The array copies only work on a pointer to a dynamic table.
	void cptrArrayNonTablePointeeFails() {
		expectLoadError(
			"array_non_table_pointee",
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type v, i64;\n"
			"    init_pany_type vp, ptr_i64;\n"
			"    ref_pptr_pany vp, v;\n"
			"    init_pany_type n, i64;\n"
			"    mov_p64_imm n, 1;\n"
			"    cptrReadArray_pptr_pcpt_p64 vp, c, n;\n"
			"    ret;\n"
			"}\n",
			{ "must point to a dynamic table" }
		);
	}

	// A table of VM pointers must not cross the native boundary.
	void cptrArrayPointerElementFails() {
		expectLoadError(
			"array_pointer_element",
			"type dynamic_table: dyn_ptr ptr_i64\n"
			"type pointer: pdynp dyn_ptr\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, cptr;\n"
			"    init_pany_type t, pdynp;\n"
			"    init_pany_type n, i64;\n"
			"    mov_p64_imm n, 1;\n"
			"    cptrWriteArray_pcpt_pptr_p64 c, t, n;\n"
			"    ret;\n"
			"}\n",
			{ "trivially copyable" }
		);
	}

	// Pointer arithmetic never changes the cpointer type; conversions go through cptrCast.
	void cptrAddOffsetAcrossTypesFails() {
		expectLoadError(
			"add_offset_across_types",
			"type cpointer: APtr i64\n"
			"type cpointer: BPtr i32\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type a, APtr;\n"
			"    init_pany_type b, BPtr;\n"
			"    init_pany_type off, i64;\n"
			"    mov_p64_imm off, 8;\n"
			"    cptrAddOffset_pcpt_pcpt_p64 a, b, off;\n"
			"    ret;\n"
			"}\n",
			{ "C pointer type does not match" }
		);
	}

	// The raw copy builtins are gone; the cptrRead/cptrWrite instructions replace them.
	void cptrCopyBuiltinsRemoved() {
		expectLoadError(
			"builtins_removed",
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    call_builtinfunc builtin_cptr_read_pptr;\n"
			"    ret;\n"
			"}\n",
			{ "not builtin" }
		);
	}

	void duplicateFfiFunctionFails() {
		// Injecting a declaration into a program that already has it clashes across load calls.
		auto file = writeTempDbc(
			"duplicate", ffiObjectHeader() + "ffi function ffi_add { i64, i64 } -> { i64 };\n"
		);
		auto pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		auto second = vm::api::loadFiles(pid, { file });
		ASSERT_NO_VALUE(second);
		ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(second.error()));
		auto why = std::get<vm::api::LoadProgramError>(second.error()).why;
		std::cerr << why << '\n';
		assertTrue(
			why.find("FFI function with this name already exists") != std::string::npos,
			base::strConcat("Unexpected error: ", why)
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/ffi/");
