// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/memory/local_slot_block.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>

#include <array>
#include <cstring>

/**
 * @brief What belongs here: memory management that must WORK while a program runs. That covers
 * destructors running exactly once and the blocks owned by variants being released.
 *
 * Memory errors the VM must detect (leaks, double frees, invalid frees, use after free) go to
 * `runtime/errors/`.
 */
class VmMemoryTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmMemoryTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noDoubleDestructorCalls);
		TESTER_ADD_TEST(variantDestructor);
		TESTER_ADD_TEST(vmValueSlots);
	}

private:
	/// Copying a VMValue into or out of a variable gives the variable a block only when its type
	/// needs one.
	void vmValueSlots() {
		vm::SafeVMProcess process(vm::PID::fromU64(0), false, false);
		ASSERT_HAS_VALUE(process.doRequest(vm::api::request::LoadFiles{
			{ fs::File(path("variant_destructor.dbc")) } }));
		const auto& types = process.getLoadedProgram()->getTypes();

		std::array<byte, 64> number_bytes{};
		vm::LocalSlot        number_slot{ .type  = types.at(base::StrID("i64")).get(),
			                              .data  = number_bytes.data(),
			                              .block = nullptr };
		const i64            seven = 7;
		std::memcpy(number_bytes.data(), &seven, sizeof(seven));

		auto number = process.createOwnedVMValue(number_slot);
		ASSERT_EQUAL(number->readBytes<i64>(), 7);
		number->writeBytes<i64>(42);
		number->exportToSlot(number_slot);
		i64 exported = 0;
		std::memcpy(&exported, number_bytes.data(), sizeof(exported));
		ASSERT_EQUAL(exported, 42);
		ASSERT_TRUE(number_slot.block == nullptr);

		// A variant keeps its set alternative in a child block of its block.
		std::array<byte, 64> variant_bytes{};
		vm::LocalSlot        variant_slot{ .type  = types.at(base::StrID("IntOrPtr")).get(),
			                               .data  = variant_bytes.data(),
			                               .block = nullptr };
		auto                 variant = process.createOwnedVMValue(variant_slot);
		ASSERT_TRUE(variant_slot.block == nullptr);
		variant->exportToSlot(variant_slot);
		ASSERT_TRUE(variant_slot.block != nullptr);

		for (auto* value: { &number, &variant })
			ASSERT_HAS_VALUE(process.doRequest(vm::api::request::FreeVMValue{ value->refMut() }));
		vm::releaseLocalSlot(variant_slot, process.getMemory());
		ASSERT_TRUE(process.getMemory().validateMemoryState());
	}

	void noDoubleDestructorCalls() {
		runTestOnVm("../../common/programs/no_double_destructor.dbc");
	}

	void variantDestructor() { runTestOnVm("variant_destructor.dbc"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/memory/");
