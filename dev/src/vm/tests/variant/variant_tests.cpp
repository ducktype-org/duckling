#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <filesystem/file_path.hpp>
#include <tester/tester.hpp>

#include <vm/bytecode/validator/errors.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/thread/vmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <limits>

class VmVariantTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmVariantTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(verySimpleVariant);
		TESTER_ADD_TEST(simpleVariant0);
		TESTER_ADD_TEST(simpleVariant1);
		TESTER_ADD_TEST(simpleVariant2);
		TESTER_ADD_TEST(blocksDontDisappearTest);
		TESTER_ADD_TEST(nestedVariantTest);
		TESTER_ADD_TEST(nestedCopy);
		TESTER_ADD_TEST(variantInsideStruct);
		TESTER_ADD_TEST(emptyVariant);
		TESTER_ADD_TEST(nonInstantiableVariant);
		TESTER_ADD_TEST(variantTypeTagTest);
	}

private:
	void verySimpleVariant() { runTestOnVm("very_simple_variant.dbc", "42", "42"); }

	void simpleVariant0() { runTestOnVm("simple_variant.dbc", "0", "13"); }

	void simpleVariant1() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "1", "13"),
			vm::exceptions::VMNullPointerCopyException::ERR_MSG
		);
	}

	void simpleVariant2() {
		assertExecutionPanickedWith(
			runTestOnVmGetResult("simple_variant.dbc", "2", "13"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void blocksDontDisappearTest() { runTestOnVm("variant_blocks_dont_disappear.dbc", "5", "5"); }

	void nestedVariantTest() {
		runTestOnVm("nested.dbc", "15", "15");
		assertExecutionPanickedWith(
			runTestOnVmGetResult("nested_failing.dbc", "15", "15"),
			vm::exceptions::VMUseAfterFreeException::ERR_MSG
		);
	}

	void nestedCopy() { runTestOnVm("nested_copy.dbc", "3", "3"); }

	void variantInsideStruct() { runTestOnVm("inside_struct.dbc"); }

	void nonInstantiableVariant() {
		loadInvalidDbc("non_instantiable.dbc", { vm::code::UninstantiableValueError::ERR_MSG });
	}

	void emptyVariant() {
		loadInvalidDbc("empty_variant.dbc", { vm::code::TooFewVariantAlternativesError::ERR_MSG });
	}

	void variantTypeTagTest() {
		/**
		 * @brief Create an owned VmValue containing a specified value.
		 */
		const auto get_int_vm_value = [&](vm::PID pid, u64 value) -> Box<vm::VmValue> {
			auto response = vm::api::getVmValue(pid, "i64");
			ASSERT_TRUE(response.has_value());
			auto vm_value = std::move(response->vm_value);
			vm_value->writeBytes<u64>(value);
			return vm_value;
		};

		auto pid = initProcess();
		vm::api::loadFiles(pid, { fs::File(path("variant_type_tag_test.dbc")) });

		auto wanted_value   = std::numeric_limits<u64>::max();
		auto vm_value_max64 = get_int_vm_value(pid, wanted_value);

		const auto assert_type_tag = [&](usize type_tag_bits, usize wanted_type_tag_value) {
			std::string function_name = base::strConcat(
				"getCustomVariant", type_tag_bits, "TypeTag", wanted_type_tag_value
			);

			ASSERT_TRUE(
				vm::api::runFunction(pid, function_name, { vm_value_max64.refMut() }).has_value()
			);
			ASSERT_TRUE(vm::api::join(pid).has_value());
			auto value = vm::api::getExitValue(pid);
			if (!value.has_value()) {
				fail(
					"Could not load VmValue for: " + function_name
					+ ", reason: " + vm::api::errorToString(value.error())
				);
			}
			ASSERT_EQUAL(value.value().size(), 1);
			const auto vm_value = value.value().at(0);
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
		};


		assert_type_tag(8, 0);
		assert_type_tag(8, 1);
		assert_type_tag(16, 0);
		assert_type_tag(16, 1);
		assert_type_tag(16, 256);
		vm_value_max64->freeData();
		vm::api::deinitAndValidate(pid);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/variant/");
