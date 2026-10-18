// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/safe/exceptions.hpp>

#include <array>
#include <string>
#include <string_view>
#include <utility>

/**
 * @brief What belongs here: every error the VM detects WHILE a single-threaded program runs. The
 * program loads fine, then the VM stops it with a panic (or reports a leak when the process is
 * validated). Each test checks that the right error is raised.
 *
 * The fixtures sit in one folder per area: `arithmetic/`, `memory/`, `pointers/`, `arrays/`,
 * `variants/` and `inheritance/`. Errors that need several threads go to `threads/`, which has
 * its own binary. A program the loader must reject goes to `loader/`.
 */
class VmRuntimeErrorsTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeErrorsTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// Arithmetic
		TESTER_ADD_TEST(checkZeroDivision);

		// Memory
		TESTER_ADD_TEST(localLeakTest);
		TESTER_ADD_TEST(globalLeakTest);
		TESTER_ADD_TEST(nestedLeaks);
		TESTER_ADD_TEST(doubleFreeIsRefused);
		TESTER_ADD_TEST(dynTableReAllocAfterFreeIsRefused);
		TESTER_ADD_TEST(dynTableFreeAfterFreeIsRefused);
		TESTER_ADD_TEST(freeingALocalIsRefused);
		TESTER_ADD_TEST(freeingAGlobalIsRefused);
		TESTER_ADD_TEST(freeingAVariantPayloadIsRefused);
		TESTER_ADD_TEST(freeingAStructFieldIsRefused);
		TESTER_ADD_TEST(readingANestedBlockAfterFreeIsRefused);

		// Pointers
		TESTER_ADD_TEST(ptrPartsTest);

		// C pointers
		TESTER_ADD_TEST(cptrNullTypedLoadFails);
		TESTER_ADD_TEST(cptrNullRawReadFails);
		TESTER_ADD_TEST(cptrCastOfNullPointerFails);

		// Arrays
		TESTER_ADD_TEST(dynTableTooLarge);
		TESTER_ADD_TEST(fstToDynOutOfBounds);
		TESTER_ADD_TEST(reallocOnFst);

		// Variants
		TESTER_ADD_TEST(simpleVariant1);
		TESTER_ADD_TEST(simpleVariant2);
		TESTER_ADD_TEST(nestedVariantUseAfterFree);

		// Inheritance
		TESTER_ADD_TEST(unsetVtable);
	}

private:
	// Arithmetic

	void checkZeroDivision() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("arithmetic/zero_division_i64.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("arithmetic/zero_division_i32.dbc", "", "0"),
			vm::exceptions::VMZeroDivisionException::ERR_MSG
		);
	}

	// Memory

	void localLeakTest() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("../../common/programs/local_leak.dbc"),
			vm::exceptions::VMFoundMemoryLeakException::ERR_MSG
		);
	}

	void globalLeakTest() {
		const auto result = runTestOnVmGetResult("../../common/programs/global_leak.dbc");
		ASSERT_HAS_VALUE(result.run_result);
		const auto validation_result = vm::api::deinitAndValidate(result.pid);
		ASSERT_HAS_VALUE(validation_result);
		ASSERT_TRUE(validation_result.value().memory_valid == false);
	}

	void nestedLeaks() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/nested_leaks.dbc"),
			vm::exceptions::VMFoundMemoryLeakException::ERR_MSG
		);
	}

	void doubleFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/double_free.dbc"),
			vm::exceptions::VMDoubleFreeException::ERR_MSG
		);
	}

	void dynTableReAllocAfterFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/dyn_table_realloc_after_free.dbc"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void dynTableFreeAfterFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/dyn_table_free_after_free.dbc"),
			vm::exceptions::VMDoubleFreeException::ERR_MSG
		);
	}

	void freeingALocalIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/free_local.dbc"),
			vm::exceptions::VMInvalidFreeException::ERR_MSG
		);
	}

	void freeingAGlobalIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/free_global.dbc"),
			vm::exceptions::VMInvalidFreeException::ERR_MSG
		);
	}

	/// A payload is a view into the variant, so freeing it would leave the parent holding a
	/// deallocated child.
	void freeingAVariantPayloadIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/free_variant_payload.dbc"),
			vm::exceptions::VMInvalidFreeException::ERR_MSG
		);
	}

	/// Freeing an interior pointer would free the whole allocation behind it.
	void freeingAStructFieldIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/free_struct_field.dbc", "0"),
			vm::exceptions::VMInvalidFreeException::ERR_MSG
		);
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/free_struct_field.dbc", "1"),
			vm::exceptions::VMInvalidFreeException::ERR_MSG
		);
	}

	/// A freed variant must not hand out its nested block, whose pool slot is already reusable.
	void readingANestedBlockAfterFreeIsRefused() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("memory/read_nested_after_free.dbc"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	// Pointers

	// The offsets of the struct and of its second field, the two ids being equal, and then the
	// null pointer failing to decompose the way a dereference does.
	void ptrPartsTest() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("pointers/ptr_parts.dbc", "", "080"), "Accessing null pointer"
		);
	}

	// C pointers

	// A default-initialized cpointer is null, so a typed load must fail cleanly, not crash.
	void cptrNullTypedLoadFails() {
		auto pid  = initProcess();
		auto file = inlineDbc(
			"null_typed_load",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, I64Ptr;\n"
			"    init_pany_type v, i64;\n"
			"    load_pany_pcptr v, p;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		ASSERT_HAS_VALUE(load, "Load failed: ", errorOf(load));
		assertExecutionPanickedWithAndKill(runTestOnVmGetResult(pid), "Accessed null CPointer");
	}

	// The same for the raw byte copy.
	void cptrNullRawReadFails() {
		auto pid  = initProcess();
		auto file = inlineDbc(
			"null_raw_read",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type c, I64Ptr;\n"
			"    init_pany_type out, i64;\n"
			"    init_pany_type op, ptr_i64;\n"
			"    ref_pptr_pany op, out;\n"
			"    read_pptr_pcptr op, c;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		ASSERT_HAS_VALUE(load, "Load failed: ", errorOf(load));
		assertExecutionPanickedWithAndKill(runTestOnVmGetResult(pid), "Accessed null CPointer");
	}

	// A null VM pointer has no block, so there is no address to take.
	void cptrCastOfNullPointerFails() {
		auto pid  = initProcess();
		auto file = inlineDbc(
			"cptr_cast_null",
			"type cpointer: I64Ptr i64\n"
			"function main { i64, ptr_argv } -> { i64 } {\n"
			"    init_pany_type p, ptr_i64;\n"
			"    setNull_pptr p;\n"
			"    init_pany_type c, I64Ptr;\n"
			"    cast_pcptr_pptr c, p;\n"
			"    ret;\n"
			"}\n"
		);
		auto load = vm::api::loadFiles(pid, { file });
		ASSERT_HAS_VALUE(load, "Load failed: ", errorOf(load));
		assertExecutionPanickedWithAndKill(runTestOnVmGetResult(pid), "Accessing null pointer");
	}

	// Arrays

	void dynTableTooLarge() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("arrays/too_large.dbc", "", "9223372036854775808"),
			vm::exceptions::VMMemoryAllocationError::ERR_MSG
		);
	}

	void fstToDynOutOfBounds() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("../../common/programs/fst_to_dyn.dbc", "1"),
			vm::exceptions::VMOutOfBlockBoundsException::ERR_MSG
		);
	}

	void reallocOnFst() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("arrays/realloc_on_fst.dbc", ""),
			vm::exceptions::VMDynTableReAllocTypeMismatch::ERR_MSG
		);
	}

	// Variants

	void simpleVariant1() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("../../common/programs/simple_variant.dbc", "1", "13"),
			vm::exceptions::VMNullPointerAccessException::ERR_MSG
		);
	}

	void simpleVariant2() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("../../common/programs/simple_variant.dbc", "2", "13"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void nestedVariantUseAfterFree() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("variants/nested_failing.dbc", "15", "15"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	// Inheritance

	void unsetVtable() {
		using namespace vm::exceptions;
		auto invalid_filename_and_error = std::to_array<std::pair<std::string, std::string_view>>({
			{
				"inheritance/unset_vtable.dbc",
				VMVtableUnset::ERR_MSG,
			},
			{
				"inheritance/unset_variant_of_virtual.dbc",
				VMVtableUnset::ERR_MSG,
			},
			{
				"inheritance/unset_table_of_virtual.dbc",
				VMVtableUnset::ERR_MSG,
			},
			{
				"inheritance/resetting_vtable.dbc",
				VMVtableUnset::ERR_MSG,
			},
		});

		for (auto& [filename, error]: invalid_filename_and_error)
			assertExecutionPanickedWithAndKill(runTestOnVmGetResult(filename, "", ""), error);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/errors/");
