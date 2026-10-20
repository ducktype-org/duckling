// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <os_utils/system_libraries.hpp>

#include <vm/api/vm.hpp>

#ifndef FFI_TEST_LIB_PATH
	#error "FFI_TEST_LIB_PATH must be defined to the built shared object path"
#endif

namespace {
	// Absolute path to the shared object built alongside this test.
	const std::string SO_PATH = FFI_TEST_LIB_PATH;

	std::string ffiObjectHeader() { return "ffi object \"" + SO_PATH + "\";\n"; }

	// Bare soname of the system math library, resolved via the platform's dynamic loader search.
	const std::string SYSTEM_MATH_LIB = os_utils::systemSharedLibM();
}

/**
 * @brief What belongs here: calling functions from a shared library through the FFI while a
 * program runs, and working with C pointers (`cptr`) that point to native memory. The programs are
 * written inline, because they name the test library by its build-dependent path.
 */
class VmFfiTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFfiTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(addPrimitives);
		TESTER_ADD_TEST(voidAndStateReadback);
		TESTER_ADD_TEST(smallIntReturnWidening);
		TESTER_ADD_TEST(cptrRawReadWrite);
		TESTER_ADD_TEST(cptrStructField);
		TESTER_ADD_TEST(cptrCopyIntoStructField);
		TESTER_ADD_TEST(cptrNullCompare);
		TESTER_ADD_TEST(cptrTypedLoadStoreMallocWorkflow);
		TESTER_ADD_TEST(cptrAddOffsetArrayWalk);
		TESTER_ADD_TEST(cptrCastFromPointer);
		TESTER_ADD_TEST(cptrCastKeepsPointerOffset);
		TESTER_ADD_TEST(floatArgsAndReturn);
		TESTER_ADD_TEST(doubleArgsAndReturn);
		TESTER_ADD_TEST(mixedIntFloatArgs);
		TESTER_ADD_TEST(floatStructByValue);
		TESTER_ADD_TEST(systemLibraryViaApi);
		TESTER_ADD_TEST(bareSonameInBytecode);
		TESTER_ADD_TEST(alignedStructByValue);
		TESTER_ADD_TEST(structWithNestedTablesByValue);
		TESTER_ADD_TEST(structWithTableOfStructsByValue);
		TESTER_ADD_TEST(structWithFloatTableByValue);
		TESTER_ADD_TEST(structWithTableReturnedFromC);
		TESTER_ADD_TEST(nestedStructByValue);
		TESTER_ADD_TEST(typedCPointerStructFieldThroughC);
		TESTER_ADD_TEST(forwardDeclaredPointee);
	}

private:
	// Loads a program expected to be valid and runs `main`, asserting the produced output.
	void runProgram(
		const std::string& name, const std::string& bytecode, const std::string& expected_output
	) {
		runTestOnVm(spawnAndLoad(inlineDbc(name, bytecode)), {}, expected_output);
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

	// Byte copies between native memory and the VM memory under a pointer, via the
	// write/read instructions. Both sides must agree on the pointee type.
	void cptrRawReadWrite() {
		runProgram(
			"cptr_raw_copy",
			ffiObjectHeader()
				+ "type cpointer: I64Ptr i64\n"
				  "ffi function ffi_alloc8 { } -> { I64Ptr };\n"
				  "ffi function ffi_free8 { I64Ptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, I64Ptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 555;\n"
				  "    init_pany_type vp, ptr_i64;\n"
				  "    ref_pptr_pany vp, v;\n"
				  "    write_pcptr_pptr buf, vp;\n"
				  "    init_pany_type out, i64;\n"
				  "    init_pany_type op, ptr_i64;\n"
				  "    ref_pptr_pany op, out;\n"
				  "    read_pptr_pcptr op, buf;\n"
				  "    output_p64 out;\n"
				  "    init_pany_type buf_f, I64Ptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
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
				  "    mov_pcptr_pcptr buf2, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 30;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    init_pany_type bufc, cptr;\n"
				  "    mov_pcptr_pcptr bufc, buf;\n"
				  "    init_pany_type v2, i64;\n"
				  "    mov_p64_imm v2, 12;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type pair, CPair;\n"
				  "    structStore_pste_pany_field pair, bufc, CPair.p;\n"
				  "    structStore_pste_pany_field pair, v2, CPair.v;\n"
				  "    call_ffifunc ffi_cpair_sum;\n"
				  "    output_p64 res;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
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
				  "type cpointer: I64Ptr i64\n"
				  "ffi function ffi_alloc8 { } -> { I64Ptr };\n"
				  "ffi function ffi_fill8 { I64Ptr, i64 } -> { };\n"
				  "ffi function ffi_free8 { I64Ptr } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type buf, I64Ptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type buf2, I64Ptr;\n"
				  "    mov_pcptr_pcptr buf2, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 4242;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    init_pany_type s, Pair;\n"
				  "    init_pany_type bp, ptr_i64;\n"
				  "    structLea_pptr_pste_field bp, s, Pair.b;\n"
				  "    read_pptr_pcptr bp, buf;\n"
				  "    init_pany_type out, i64;\n"
				  "    structLoad_pany_pste_field out, s, Pair.b;\n"
				  "    output_p64 out;\n"
				  "    init_pany_type buf_f, I64Ptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    ret;\n"
				  "}\n",
			"4242"
		);
	}

	// `cmpNull_pcptr` sets the flag on a null cpointer (default-initialized here) and clears it
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
				  "    cmpNull_pcptr nul;\n"
				  "    cmov_p64_imm a, 1;\n"
				  "    output_p64 a;\n"
				  "    init_pany_type buf, cptr;\n"
				  "    call_ffifunc ffi_alloc8;\n"
				  "    init_pany_type b, i64;\n"
				  "    mov_p64_imm b, 0;\n"
				  "    cmpNull_pcptr buf;\n"
				  "    cmov_p64_imm b, 1;\n"
				  "    output_p64 b;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
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

	// A system library injected through `loadCode` (the path the `--ffi-lib` CLI option uses)
	// must be visible to `ffi function` declarations from a later `loadFiles` call.
	void systemLibraryViaApi() {
		auto pid = initProcess();

		vm::code::CodeCollection libs;
		libs.object_files.emplace_back(SYSTEM_MATH_LIB);
		assertSucceeded(vm::api::loadCode(pid, libs), "Loading the system library");

		auto file = inlineDbc(
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
		assertSucceeded(vm::api::loadFiles(pid, { file }), "Load of 'system_lib_api'");
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
				  "    mov_pcptr_pcptr bufc, buf;\n"
				  "    init_pany_type tag, i64;\n"
				  "    mov_p64_imm tag, 42;\n"
				  "    init_pany_type t, Tagged;\n"
				  "    structStore_pste_pany_field t, bufc, Tagged.p;\n"
				  "    structStore_pste_pany_field t, tag, Tagged.tag;\n"
				  "    call_ffifunc ffi_tagged_store;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type buf2, I64Ptr;\n"
				  "    mov_pcptr_pcptr buf2, buf;\n"
				  "    call_ffifunc ffi_read8;\n"
				  "    output_p64 res;\n"
				  "    init_pany_type buf3, I64Ptr;\n"
				  "    mov_pcptr_pcptr buf3, buf;\n"
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
				  "    mov_pcptr_pcptr h2, h;\n"
				  "    call_ffifunc ffi_free8;\n"
				  "    init_pany_type ok, i64;\n"
				  "    mov_p64_imm ok, 1;\n"
				  "    output_p64 ok;\n"
				  "    ret;\n"
				  "}\n",
			"1"
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
				  "    cast_pcptr_pcptr pp, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    init_pany_type s, Pair;\n"
				  "    mov_p64_imm v, 30;\n"
				  "    structStore_pste_pany_field s, v, Pair.a;\n"
				  "    mov_p64_imm v, 12;\n"
				  "    structStore_pste_pany_field s, v, Pair.b;\n"
				  "    store_pcptr_pany pp, s;\n"
				  "    init_pany_type s2, Pair;\n"
				  "    load_pany_pcptr s2, pp;\n"
				  "    init_pany_type x, i64;\n"
				  "    init_pany_type sum, i64;\n"
				  "    structLoad_pany_pste_field sum, s2, Pair.a;\n"
				  "    structLoad_pany_pste_field x, s2, Pair.b;\n"
				  "    add_p64_p64 sum, x;\n"
				  "    output_p64 sum;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
				  "    call_ffifunc ffi_free;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// Walks a C array with byte-wise pointer arithmetic. Both offset forms advance the cpointer
	// in place, so each step works on its own copy of the base pointer.
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
				  "    mov_p64_imm n, 24;\n"
				  "    call_ffifunc ffi_alloc;\n"
				  "    init_pany_type p, I64Ptr;\n"
				  "    cast_pcptr_pcptr p, buf;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 20;\n"
				  "    store_pcptr_pany p, v;\n"
				  "    init_pany_type off, i64;\n"
				  "    mov_p64_imm off, 8;\n"
				  "    init_pany_type p2, I64Ptr;\n"
				  "    mov_pcptr_pcptr p2, p;\n"
				  "    add_pcptr_p64 p2, off;\n"
				  "    mov_p64_imm v, 2;\n"
				  "    store_pcptr_pany p2, v;\n"
				  "    init_pany_type p3, I64Ptr;\n"
				  "    mov_pcptr_pcptr p3, p;\n"
				  "    add_pcptr_imm p3, 16;\n"
				  "    mov_p64_imm v, 20;\n"
				  "    store_pcptr_pany p3, v;\n"
				  "    init_pany_type x0, i64;\n"
				  "    load_pany_pcptr x0, p;\n"
				  "    init_pany_type x1, i64;\n"
				  "    load_pany_pcptr x1, p2;\n"
				  "    init_pany_type x2, i64;\n"
				  "    load_pany_pcptr x2, p3;\n"
				  "    add_p64_p64 x0, x1;\n"
				  "    add_p64_p64 x0, x2;\n"
				  "    output_p64 x0;\n"
				  "    init_pany_type buf_f, cptr;\n"
				  "    mov_pcptr_pcptr buf_f, buf;\n"
				  "    call_ffifunc ffi_free;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// Hands the native address of a VM local to C, which writes through it. The VM sees the
	// updated value because the cpointer addresses the block's data directly.
	void cptrCastFromPointer() {
		runProgram(
			"cptr_cast_from_pointer",
			ffiObjectHeader()
				+ "type cpointer: I64Ptr i64\n"
				  "ffi function ffi_fill8 { I64Ptr, i64 } -> { };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 0;\n"
				  "    init_pany_type vp, ptr_i64;\n"
				  "    ref_pptr_pany vp, v;\n"
				  "    init_pany_type c, I64Ptr;\n"
				  "    cast_pcptr_pptr c, vp;\n"
				  "    init_pany_type x, i64;\n"
				  "    mov_p64_imm x, 42;\n"
				  "    call_ffifunc ffi_fill8;\n"
				  "    output_p64 v;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}

	// The cast keeps the pointer's offset, so a field address computed in the VM stays a field
	// address on the native side.
	void cptrCastKeepsPointerOffset() {
		runProgram(
			"cptr_cast_offset",
			ffiObjectHeader()
				+ "type data: Pair { a: i64, b: i64 } assert_size 16\n"
				  "type pointer: PairPtr Pair\n"
				  "type cpointer: I64CPtr i64\n"
				  "ffi function ffi_read8 { I64CPtr } -> { i64 };\n"
				  "function main { i64, ptr_argv } -> { i64 } {\n"
				  "    init_pany_type s, Pair;\n"
				  "    init_pany_type v, i64;\n"
				  "    mov_p64_imm v, 7;\n"
				  "    structStore_pste_pany_field s, v, Pair.a;\n"
				  "    mov_p64_imm v, 42;\n"
				  "    structStore_pste_pany_field s, v, Pair.b;\n"
				  "    init_pany_type sp, PairPtr;\n"
				  "    ref_pptr_pany sp, s;\n"
				  "    init_pany_type fp, ptr_i64;\n"
				  "    structLea_pptr_pptr_field fp, sp, Pair.b;\n"
				  "    init_pany_type res, i64;\n"
				  "    init_pany_type c, I64CPtr;\n"
				  "    cast_pcptr_pptr c, fp;\n"
				  "    call_ffifunc ffi_read8;\n"
				  "    output_p64 res;\n"
				  "    ret;\n"
				  "}\n",
			"42"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/native/ffi/");
