// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/safe_vmprocess.hpp>

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
		TESTER_ADD_TEST(vmValueBlocks);
	}

private:
	/// A VMValue gets a memory block only when its data needs one, and reading never needs one.
	void vmValueBlocks() {
		vm::SafeVMProcess process(vm::PID::fromU64(0), false, false);
		ASSERT_HAS_VALUE(process.doRequest(vm::api::request::LoadFiles{
			{ fs::File(path("variant_destructor.dbc")) } }));
		const auto& types = process.getLoadedProgram()->getTypes();

		auto number = process.createOwnedVMValue(types.at(base::StrID("i64")));
		number->writeBytes<i64>(42);
		ASSERT_EQUAL(number->readBytes<i64>(), 42);
		const vm::IVMValue& number_value = *number;
		ASSERT_EQUAL(
			number_value.readData<vm::interpreted_data_variant::Primitive>().value().value, 42
		);
		ASSERT_TRUE(!number->hasBlock());

		auto copy = process.createOwnedVMValue(types.at(base::StrID("i64")));
		copy->importDataFrom(*number);
		ASSERT_EQUAL(copy->readBytes<i64>(), 42);
		ASSERT_TRUE(!copy->hasBlock());

		// Bigger than the inline buffer. Plain bytes have no view block to type the payload.
		const auto variant_type = types.at(base::StrID("IntOrPtr"));
		const auto tag_size     = static_cast<usize>(
            std::get<vm::kind::Variant>(variant_type->getKindVariant()).type_tag_size
        );
		auto variant = process.createOwnedVMValue(variant_type);
		std::memset(variant->getBytes(), 1, 1);  // The `i64` alternative, as tags start at 1.
		const i64 payload = 42;
		std::memcpy(variant->getBytes() + tag_size, &payload, sizeof(payload));
		ASSERT_EQUAL(variant->getBytes()[0], byte{ 1 });
		ASSERT_TRUE(!variant->readData().has_value());
		ASSERT_TRUE(!variant->hasBlock());

		for (auto* value: { &number, &copy, &variant }) {
			ASSERT_HAS_VALUE(process.doRequest(vm::api::request::FreeVMValue{ value->refMut() }));
			ASSERT_TRUE((*value)->isFreed());
		}
		ASSERT_TRUE(process.getMemory().validateMemoryState());

		bool threw = false;
		try {
			(void) number_value.readData();
		} catch (const vm::exceptions::VMUseAfterFreeException&) { threw = true; }
		ASSERT_TRUE(threw);
	}

	void noDoubleDestructorCalls() {
		runTestOnVm("../../common/programs/no_double_destructor.dbc");
	}

	void variantDestructor() { runTestOnVm("variant_destructor.dbc"); }
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/memory/");
