// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/misc/int_conv.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/utils/interpret.hpp>

#include <array>
#include <limits>
#include <string>
#include <vector>

/**
 * @brief What belongs here: running one function of a loaded program through `vm::api`. That
 * covers passing it arguments as VMValues (and refusing the wrong ones), reading the values it
 * returns (several of them, or a variant with its type tag), and running it several times in a row.
 *
 * Loading more code into a process between the runs goes to `api/injection/`. The endpoint
 * contract of `runFunction` and `runFunctionAwait` stays in `api/api_test.cpp`, since part of it
 * runs an endless function, which the TC flavour can only run in an optimised build.
 */
class VmRunFunctionTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRunFunctionTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(runNoArgFunction);
		TESTER_ADD_TEST(runVoidFunction);
		TESTER_ADD_TEST(runNonVoidFunction);
		TESTER_ADD_TEST(runMultipleReturnValuesFunction);
		TESTER_ADD_TEST(doubleRunFunction);
		TESTER_ADD_TEST(manyRunFunctions);
		TESTER_ADD_TEST(runFunctionArgumentValidation);
		TESTER_ADD_TEST(testSyncRun);
		TESTER_ADD_TEST(variantTypeTagTest);
	}

private:
	/**
	 * @brief An IVMValue implementation which does not come from the safe VM. Used to check that
	 * the safe VM rejects values it did not create, even when their PID matches.
	 */
	class ForeignVMValue final: public vm::IVMValue {
	public:
		explicit ForeignVMValue(vm::PID pid): pid(pid) {}

		void freeData() override {}

		[[nodiscard]] base::CRef<vm::code::valid_type::ValidType> getType() const override {
			CORE_PANIC("ForeignVMValue has no type");
		}

		[[nodiscard]] vm::code::valid_type::ValidTypeID getTypeID() const override {
			CORE_PANIC("ForeignVMValue has no type");
		}

		[[nodiscard]] Bytes getDataSize() const override { return Bytes(sizeof(data)); }

		[[nodiscard]] base::Optional<vm::InterpretedDataVariant> readData() const override {
			return {};
		}

		void importDataFrom(const IVMValue&) override {
			CORE_PANIC("ForeignVMValue cannot import data");
		}

		[[nodiscard]] vm::PID getPID() const override { return pid; }

		[[nodiscard]] byte* getBytes() override { return data.data(); }

		[[nodiscard]] const byte* getBytes() const override { return data.data(); }

		void dprint(std::ostream& out, const std::string& indent) const override {
			out << indent << "ForeignVMValue\n";
		}

	private:
		vm::PID pid;

		std::array<byte, sizeof(i64)> data{};
	};

	void runNoArgFunction() {
		vm::PID pid = spawnAndLoad("call_no_arg_function.dbc");

		runAndCheckReturnValue(pid, "summer", vm::FunctionRunArguments{}, {}, "735", {});
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void runVoidFunction() {
		vm::PID pid = spawnAndLoad("call_void_function.dbc");

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 1, 2 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, "3", {});
		freeArguments(owned_arguments);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void runNonVoidFunction() {
		vm::PID pid = spawnAndLoad("call_non_void_function.dbc");

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 695, 40 });
		runAndCheckReturnValue(pid, "summer", createArgumentList(owned_arguments), {}, {}, 735);
		freeArguments(owned_arguments);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void runMultipleReturnValuesFunction() {
		vm::PID pid = spawnAndLoad("multiple_retvals.dbc");

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 40, 695 });
		runAndCheckReturnValues(
			pid,
			"unsafe_div",
			createArgumentList(owned_arguments),
			{},
			{},
			base::Optional<std::vector<i64>>{ { 17, 15 } }
		);

		// Double run to check if the frame is reset correctly.
		runAndCheckReturnValues(
			pid,
			"unsafe_div",
			createArgumentList(owned_arguments),
			{},
			{},
			base::Optional<std::vector<i64>>{ { 17, 15 } }
		);
		freeArguments(owned_arguments);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void doubleRunFunction() {
		vm::PID pid = spawnAndLoad("../../common/programs/repl_1.dbc");

		OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { 4, 8 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments), {}, {}, 32);
		OwnedArgumentList owned_arguments2 = getOwnedArgumentList(pid, { 4, 6 });
		runAndCheckReturnValue(pid, "spring", createArgumentList(owned_arguments2), {}, {}, 24);
		freeArguments(owned_arguments);
		freeArguments(owned_arguments2);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void manyRunFunctions() {
		vm::PID pid = spawnAndLoad("../../common/programs/repl_1.dbc");

		for (i32 i = 0; i < 100; i++) {
			OwnedArgumentList owned_arguments = getOwnedArgumentList(pid, { i, i });
			runAndCheckReturnValue(
				pid, "spring", createArgumentList(owned_arguments), {}, {}, i * i
			);
			freeArguments(owned_arguments);
		}
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void runFunctionArgumentValidation() {
		vm::PID pid = spawnAndLoad("call_non_void_function.dbc");
		{
			// Not enough arguments.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVMValue(pid, 10));
			auto func_args = createArgumentList(arguments);

			assertRunFunctionRefusedWith(
				pid, "summer", func_args, "expects 2 arguments, but 1 were provided"
			);

			freeArguments(arguments);
		}
		{
			// Too many arguments.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVMValue(pid, 10));
			arguments.push_back(getIntVMValue(pid, 20));
			arguments.push_back(getIntVMValue(pid, 30));
			auto func_args = createArgumentList(arguments);

			assertRunFunctionRefusedWith(
				pid, "summer", func_args, "expects 2 arguments, but 3 were provided"
			);

			freeArguments(arguments);
		}

		{
			// Argument type mismatch.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVMValue(pid, 10));

			auto i32_value = vm::api::getVMValue(pid, "i32");
			ASSERT_HAS_VALUE(i32_value);
			arguments.push_back(std::move(i32_value->vm_value));

			auto func_args = createArgumentList(arguments);

			assertRunFunctionRefusedWith(
				pid,
				"summer",
				func_args,
				"Type mismatch for argument 1 of function 'summer': expected i64, got i32"
			);

			freeArguments(arguments);
		}

		{
			// VMValue with a matching PID, but not created by the safe VM implementation.
			OwnedArgumentList arguments;
			arguments.push_back(getIntVMValue(pid, 5));
			arguments.push_back(Box<vm::IVMValue>::fromPointer(new ForeignVMValue(pid)));
			auto func_args = createArgumentList(arguments);

			assertRunFunctionRefusedWith(
				pid,
				"summer",
				func_args,
				"VMValue for argument 1 is invalid: it does not belong to the safe VM "
				"implementation"
			);

			freeArguments(arguments);
		}

		{
			// VMValue from different process.
			vm::PID other_pid = initProcess();

			OwnedArgumentList arguments;
			arguments.push_back(getIntVMValue(pid, 5));
			arguments.push_back(getIntVMValue(other_pid, 99));
			auto func_args = createArgumentList(arguments);

			assertRunFunctionRefusedWith(
				pid, "summer", func_args, "VMValue for argument 1 comes from a different process"
			);

			freeArguments(arguments);
			assertSucceeded(vm::api::deinitAndValidate(other_pid), "deinitAndValidate");
		}

		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

	void testSyncRun() {
		vm::PID    pid        = spawnAndLoad("simple_function.dbc");
		const auto exit_value = vm::api::runFunctionAwait(pid, "foo", {});
		ASSERT_HAS_VALUE(exit_value);
		ASSERT_NO_VALUE(vm::api::join(pid));

		auto output = vm::api::output(pid);
		ASSERT_HAS_VALUE(output);
		ASSERT_EQUAL(std::string("120"), output->output);

		ASSERT_MATCHES(exit_value.value(), std::vector<Ref<vm::IVMValue>>);
		const auto& values = std::get<std::vector<Ref<vm::IVMValue>>>(exit_value.value());
		ASSERT_EQUAL_PRINT(values.size(), 1);
		ASSERT_EQUAL_PRINT(values.at(0)->readBytes<i64>(), 123);
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}

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

		auto pid = spawnAndLoad("variant_type_tag_test.dbc");

		auto wanted_value   = std::numeric_limits<u64>::max();
		auto vm_value_max64 = get_int_vm_value(pid, wanted_value);

		const auto assert_type_tag = [&](usize type_tag_bits, usize alternative_index) {
			const usize wanted_type_tag_value = alternative_index + 1;
			std::string function_name         = base::strConcat(
                "getCustomVariant", type_tag_bits, "Alternative", alternative_index
            );

			assertSucceeded(
				vm::api::runFunction(pid, function_name, { vm_value_max64.refMut() }), "runFunction"
			);
			auto value = vm::api::join(pid);
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
		assertSucceeded(vm::api::deinitAndValidate(pid), "deinitAndValidate");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/run_function/");
