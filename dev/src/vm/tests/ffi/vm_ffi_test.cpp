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
}

class VmFfiTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFfiTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(addPrimitives);
		TESTER_ADD_TEST(voidAndStateReadback);
		TESTER_ADD_TEST(smallIntReturnWidening);
		TESTER_ADD_TEST(cptrRoundTripThroughC);
		TESTER_ADD_TEST(cptrCopyBuiltins);
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
		TESTER_ADD_TEST(duplicateFfiFunctionFails);
		TESTER_ADD_TEST(assertSizeMatches);
		TESTER_ADD_TEST(assertSizeMismatchFails);
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

	void cptrRoundTripThroughC() {
		runProgram(
			"cptr_c",
			ffiObjectHeader()
				+ "ffi function ffi_alloc8 { } -> { cptr };\n"
				  "ffi function ffi_fill8 { cptr, i64 } -> { };\n"
				  "ffi function ffi_read8 { cptr } -> { i64 };\n"
				  "ffi function ffi_free8 { cptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type buf2, cptr;\n"
				  "    mov_popq_popq buf2, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 777;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type buf3, cptr;\n"
				  "    mov_popq_popq buf3, buf;\n"
				  "    call_ffifunc ffi_read8;\n"
				  "    output_p64 res;\n"
				  "    init_pany_type buf4, cptr;\n"
				  "    mov_popq_popq buf4, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"777"
		);
	}

	void cptrCopyBuiltins() {
		runProgram(
			"cptr_copy",
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
				  "    init_pany_type buf_w, cptr;\n"
				  "    mov_popq_popq buf_w, buf;\n"
				  "    init_pany_type vp2, ptr_i64;\n"
				  "    mov_pptr_pptr vp2, vp;\n"
				  "    call_builtinfunc builtin_cptr_write_pptr;\n"
				  "    init_pany_type out, i64;\n"
				  "    init_pany_type op, ptr_i64;\n"
				  "    ref_pptr_pany op, out;\n"
				  "    init_pany_type buf_r, cptr;\n"
				  "    mov_popq_popq buf_r, buf;\n"
				  "    init_pany_type op2, ptr_i64;\n"
				  "    mov_pptr_pptr op2, op;\n"
				  "    call_builtinfunc builtin_cptr_read_pptr;\n"
				  "    output_p64 out;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_popq_popq buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"555"
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
		libs.object_files.emplace_back("libm.so.6");
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
			"ffi object \"libm.so.6\";\n"
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

	void duplicateFfiFunctionFails() {
		// Duplicates within one file are deduplicated (like ordinary functions), so the clash is
		// only detected when the same declaration is injected into a program that already has it.
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
