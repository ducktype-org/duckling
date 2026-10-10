// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <filesystem/file_path.hpp>
#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <limits>

class VmVariantTest: public VmRuntimeTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_RUNTIME_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verySimpleVariant);
		TESTER_ADD_TEST(simpleVariant0);
		TESTER_ADD_TEST(simpleVariant1);
		TESTER_ADD_TEST(simpleVariant2);
		TESTER_ADD_TEST(blocksDontDisappearTest);
		TESTER_ADD_TEST(nestedVariantTest);
		TESTER_ADD_TEST(nestedCopy);
		TESTER_ADD_TEST(variantInsideStruct);
		TESTER_ADD_TEST(variantTypeTagTest);
	}

private:
	void verySimpleVariant() { runTestOnVm("very_simple_variant.dbc", "42", "42"); }

	void simpleVariant0() { runTestOnVm("simple_variant.dbc", "0", "13"); }

	void simpleVariant1() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("simple_variant.dbc", "1", "13"),
			vm::exceptions::VMNullPointerAccessException::ERR_MSG
		);
	}

	void simpleVariant2() {
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("simple_variant.dbc", "2", "13"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }

	void nestedVariantTest() {
		runTestOnVm("nested.dbc", "15", "15");
		assertExecutionPanickedWithAndKill(
			runTestOnVmGetResult("nested_failing.dbc", "15", "15"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void nestedCopy() { runTestOnVm("nested_copy.dbc", "3", "3"); }

	void variantInsideStruct() { runTestOnVm("inside_struct.dbc"); }

	void variantTypeTagTest() {
		/**
		 * @brief Create an owned VMValue containing a specified value.
		 */
		const auto get_int_vm_value = [&](vm::PID pid, u64 value) -> Box<vm::IVMValue> {
			auto response = vm::api::getVMValue(pid, "i64");
			ASSERT_HAS_VALUE(response);
			auto vm_value = std::move(response->vm_value);
			vm_value->writeBytes<u64>(value);
			return vm_value;
		};

		auto pid = initProcess();
		ASSERT_HAS_VALUE(vm::api::loadFiles(pid, { fs::File(path("variant_type_tag_test.dbc")) }));

		auto wanted_value   = std::numeric_limits<u64>::max();
		auto vm_value_max64 = get_int_vm_value(pid, wanted_value);

		const auto assert_type_tag = [&](usize type_tag_bits, usize alternative_index) {
			const usize wanted_type_tag_value = alternative_index + 1;
			std::string function_name         = base::strConcat(
                "getCustomVariant", type_tag_bits, "Alternative", alternative_index
            );

			ASSERT_HAS_VALUE(vm::api::runFunction(pid, function_name, { vm_value_max64.refMut() }));
			ASSERT_HAS_VALUE(vm::api::join(pid));
			auto value = vm::api::getExitValue(pid);
			if (!value.has_value()) {
				fail(
					"Could not load VMValue for: " + function_name
					+ ", reason: " + vm::api::errorToString(value.error())
				);
			}
			ASSERT_MATCHES(value.value(), std::vector<Ref<vm::IVMValue>>);
			auto& value_vec = std::get<std::vector<Ref<vm::IVMValue>>>(value.value());
			ASSERT_EQUAL(value_vec.size(), 1);
			const auto vm_value = value_vec.at(0);
			switch (type_tag_bits) {
			case 8:
				// Using uint8_t, because u8 is not integral
				ASSERT_EQUAL_PRINT(
					vm_value->readBytes<uint8_t>(), base::safeIntConv<uint8_t>(wanted_type_tag_value)
				);
				break;
			case 16:
				ASSERT_EQUAL_PRINT(
					vm_value->readBytes<u16>(), base::safeIntConv<u16>(wanted_type_tag_value)
				);
				break;
			default:
				CORE_PANIC("Invalid type_tag_bits: ", type_tag_bits);
			}
			ASSERT_EQUAL_PRINT(
				vm::safeReadPointerBytes<u64>(vm_value->getBytes(), type_tag_bits / 8), wanted_value
			);

			auto decoded_variant = vm_value->readData<vm::interpreted_data_variant::Variant>();
			ASSERT_HAS_VALUE(decoded_variant);
			ASSERT_EQUAL_PRINT(decoded_variant->alternative_index, alternative_index);

			if (type_tag_bits == 8)
				vm_value->writeBytes<uint8_t>(0);
			else
				vm_value->writeBytes<u16>(0);
			ASSERT_NO_VALUE(vm_value->readData<vm::interpreted_data_variant::Variant>());
		};


		auto empty_variant_response = vm::api::getVMValue(pid, "CustomVariant8");
		ASSERT_HAS_VALUE(empty_variant_response);
		auto empty_variant = std::move(empty_variant_response->vm_value);
		ASSERT_NO_VALUE(empty_variant->readData<vm::interpreted_data_variant::Variant>());
		empty_variant->freeData();

		assert_type_tag(8, 0);
		assert_type_tag(8, 1);
		assert_type_tag(8, 254);
		assert_type_tag(16, 0);
		assert_type_tag(16, 1);
		assert_type_tag(16, 255);

		auto invalid_variant_response = vm::api::getVMValue(pid, "CustomVariant16");
		ASSERT_HAS_VALUE(invalid_variant_response);
		auto invalid_variant = std::move(invalid_variant_response->vm_value);
		invalid_variant->writeBytes<u16>(257);
		assertThrows<std::out_of_range>(
			[&] {
				static_cast<void>(invalid_variant->readData<vm::interpreted_data_variant::Variant>()
			    );
			},
			"An out-of-range variant tag must not be treated as an empty variant."
		);
		invalid_variant->freeData();

		vm_value_max64->freeData();
		ASSERT_HAS_VALUE(vm::api::deinitAndValidate(pid));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/runtime/variant/");
