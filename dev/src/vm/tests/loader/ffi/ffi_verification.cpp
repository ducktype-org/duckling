// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <vm/api/vm.hpp>

#ifndef FFI_TEST_LIB_PATH
	#error "FFI_TEST_LIB_PATH must be defined to the built shared object path"
#endif

namespace {
	// Absolute path to the shared object built alongside `runtime/native/ffi/`.
	const std::string SO_PATH = FFI_TEST_LIB_PATH;

	std::string ffiObjectHeader() { return "ffi object \"" + SO_PATH + "\";\n"; }
}

/**
 * @brief What belongs here: every FFI error the loader must reject. That covers `ffi object`
 * files that cannot be loaded, symbols missing from them, and `ffi function` signatures libffi
 * cannot describe (unsupported or packed types, by-value tables, several results). The programs
 * are written inline, because they name the test library by its build-dependent path.
 *
 * Calling FFI functions while a program runs goes to `runtime/native/ffi/`.
 */
class VmFfiVerificationTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmFfiVerificationTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// Object files and symbols
		TESTER_ADD_TEST(missingSystemLibraryFails);
		TESTER_ADD_TEST(missingObjectFileFails);
		TESTER_ADD_TEST(missingSymbolFails);
		TESTER_ADD_TEST(duplicateFfiFunctionFails);

		// Signatures
		TESTER_ADD_TEST(floatNameWithWrongSizeFails);
		TESTER_ADD_TEST(unsupportedTypeFails);
		TESTER_ADD_TEST(multipleResultsFails);
		TESTER_ADD_TEST(tableByValueFails);
		TESTER_ADD_TEST(nonCompliantStructsFail);
		TESTER_ADD_TEST(structWithPackedFieldFails);
		TESTER_ADD_TEST(packedStructInFfiFails);
		TESTER_ADD_TEST(packedStructWithMatchingLayoutStillFails);
	}

private:
	// Object files and symbols

	void missingSystemLibraryFails() {
		loadInvalidDbc(
			inlineDbc(
				"missing_system_lib",
				"ffi object \"libduckling_no_such_lib.so\";\n"
				"ffi function ffi_add { i64, i64 } -> { i64 };\n"
			),
			{ "Failed to load" }
		);
	}

	void missingObjectFileFails() {
		loadInvalidDbc(
			inlineDbc(
				"missing_object",
				"ffi object \"/definitely/not/a/real/library.so\";\n"
				"ffi function ffi_add { i64, i64 } -> { i64 };\n"
			),
			{ "Failed to load" }
		);
	}

	void missingSymbolFails() {
		loadInvalidDbc(
			inlineDbc(
				"missing_symbol", ffiObjectHeader() + "ffi function ffi_does_not_exist { } -> { };\n"
			),
			{ "Symbol not found" }
		);
	}

	void duplicateFfiFunctionFails() {
		// Injecting a declaration into a program that already has it clashes across load calls.
		auto file = inlineDbc(
			"duplicate", ffiObjectHeader() + "ffi function ffi_add { i64, i64 } -> { i64 };\n"
		);
		auto pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { file }));

		auto second = vm::api::loadFiles(pid, { file });
		ASSERT_NO_VALUE(second);
		ASSERT_MATCHES(second.error(), vm::api::LoadProgramError);
		auto why = std::get<vm::api::LoadProgramError>(second.error()).why;
		std::cerr << why << '\n';
		assertTrue(
			why.find("FFI function with this name already exists") != std::string::npos,
			base::strConcat("Unexpected error: ", why)
		);
	}

	// Signatures

	void floatNameWithWrongSizeFails() {
		loadInvalidDbc(
			inlineDbc(
				"bad_float_size",
				ffiObjectHeader()
					+ "type primitive: f32 8\n"
					  "ffi function ffi_addf { f32, f32 } -> { f32 };\n"
			),
			{ "FFI function signature" }
		);
	}

	void unsupportedTypeFails() {
		loadInvalidDbc(
			inlineDbc("bad_type", ffiObjectHeader() + "ffi function ffi_add { string } -> { };\n"),
			{ "FFI function signature" }
		);
	}

	void multipleResultsFails() {
		loadInvalidDbc(
			inlineDbc(
				"multi_result",
				ffiObjectHeader() + "ffi function ffi_add { i64, i64 } -> { i64, i64 };\n"
			),
			{ "at most one" }
		);
	}

	// C has no by-value arrays, so a fixed-size table is only allowed as a structure field.
	void tableByValueFails() {
		loadInvalidDbc(
			inlineDbc(
				"table_by_value",
				ffiObjectHeader()
					+ "type fixed_size_table: arr4 i32 4\n"
					  "ffi function ffi_add { arr4 } -> { i64 };\n"
			),
			{ "by-value arrays", "fixed-size table" }
		);
	}

	// Structures whose libffi descriptor would be degenerate are not FFI-compliant: a
	// zero-length table flattens to no elements, a field-less struct has size 0 (rejected by
	// libffi), and an oversized flattened element count must fail before it is materialized,
	// also when the elements hide inside nested structure descriptors.
	void nonCompliantStructsFail() {
		loadInvalidDbc(
			inlineDbc(
				"zero_length_table_struct",
				ffiObjectHeader()
					+ "type fixed_size_table: arr0 i32 0\n"
					  "type data: WithEmpty { v: arr0 }\n"
					  "ffi function ffi_add { WithEmpty } -> { i64 };\n"
			),
			{ "WithEmpty", "cannot be used in an FFI function signature" }
		);
		loadInvalidDbc(
			inlineDbc(
				"empty_struct",
				ffiObjectHeader()
					+ "type data: Empty { }\n"
					  "ffi function ffi_add { Empty } -> { i64 };\n"
			),
			{ "Empty", "cannot be used in an FFI function signature" }
		);
		loadInvalidDbc(
			inlineDbc(
				"huge_table_struct",
				ffiObjectHeader()
					+ "type fixed_size_table: huge i32 100000000\n"
					  "type data: WithHuge { v: huge }\n"
					  "ffi function ffi_add { WithHuge } -> { i64 };\n"
			),
			{ "WithHuge", "cannot be used in an FFI function signature" }
		);
		// Each nested struct is under the cap on its own, but their sum is not.
		loadInvalidDbc(
			inlineDbc(
				"nested_struct_cap_bypass",
				ffiObjectHeader()
					+ "type fixed_size_table: big i32 40000\n"
					  "type data: Inner { v: big }\n"
					  "type data: Outer { a: Inner, b: Inner }\n"
					  "ffi function ffi_add { Outer } -> { i64 };\n"
			),
			{ "Outer", "cannot be used in an FFI function signature" }
		);
	}

	// A packed struct is not FFI-compliant, so neither is any struct containing one.
	void structWithPackedFieldFails() {
		loadInvalidDbc(
			inlineDbc(
				"packed_field_struct",
				ffiObjectHeader()
					+ "type data: PackedInner { a: i8, b: i64 } packed\n"
					  "type data: Holder { p: PackedInner, v: i64 }\n"
					  "ffi function ffi_add { Holder } -> { i64 };\n"
			),
			{ "cannot be used in an FFI function signature", "Holder" }
		);
	}

	// libffi can only describe the C ABI layout, so packed structs are rejected in FFI
	// signatures.
	void packedStructInFfiFails() {
		loadInvalidDbc(
			inlineDbc(
				"packed_struct_ffi",
				ffiObjectHeader()
					+ "type data: Mix { a: i8, b: i64 } packed\n"
					  "ffi function ffi_mix_sum { Mix } -> { i64 };\n"
			),
			{ "packed", "cannot be used in an FFI function signature" }
		);
	}

	// Rejected even when the packed layout coincides with the C ABI layout (all fields naturally
	// aligned) - such a struct is identical to its non-packed version, so `packed` is dropped
	// rather than special-cased.
	void packedStructWithMatchingLayoutStillFails() {
		loadInvalidDbc(
			inlineDbc(
				"packed_struct_matching_ffi",
				ffiObjectHeader()
					+ "type primitive: f32 4\n"
					  "type data: FPair { a: f32, b: f32 } packed assert_size 8\n"
					  "ffi function ffi_fpair_swap { FPair } -> { FPair };\n"
			),
			{ "packed", "cannot be used in an FFI function signature" }
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/loader/ffi/");
