#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/debugger/UI/CLI/common.hpp>

class VmDebuggerCliCommonTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerCliCommonTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stripTest);
		TESTER_ADD_TEST(extractPrimitiveValuesTest);
		TESTER_ADD_TEST(statusLineTest);
		TESTER_ADD_TEST(withoutControlSequencesTest);
	}

private:
	void stripTest() {
		using vm::debugger::cli::common::strip;

		ASSERT_EQUAL_PRINT("duckling", strip("duckling"));
		ASSERT_EQUAL_PRINT("duckling", strip("duckling\n\n\n\n"));
		ASSERT_EQUAL_PRINT("duckling", strip(" duckling\t"));
		ASSERT_EQUAL_PRINT("duckling", strip("  duckling\r\r\n"));
		ASSERT_EQUAL_PRINT("duckling", strip(" \r\t\nduckling"));

		ASSERT_EQUAL_PRINT("duck\rling", strip("duck\rling"));
		ASSERT_EQUAL_PRINT("", strip("    "));
		ASSERT_EQUAL_PRINT("", strip("  \r  "));
	}

	void extractPrimitiveValuesTest() {
		using vm::debugger::cli::common::extractPrimitiveValues;

		{
			auto values = extractPrimitiveValues(vm::api::Running{});
			ASSERT_EQUAL_PRINT(0, values.size());
		}

		{
			auto values = extractPrimitiveValues(vm::api::ExecutionCompleted{
				.exit_value = 7,
			});
			ASSERT_EQUAL_PRINT(1, values.size());
			ASSERT_EQUAL_PRINT("7", values[0]);
		}

		{
			auto type = vm::Type::declareType(base::StrID("i64"));
			type.definePrimitive(Bytes(8));
			type.finalize();

			auto process   = vm::SafeVMProcess((vm::PID) 0);
			auto seven     = process.createOwnedVMValue(&type);
			auto forty_two = process.createOwnedVMValue(&type);
			seven->writeBytes<i64>(7);
			forty_two->writeBytes<i64>(42);

			vm::api::ProcStatus status = vm::api::ExecutionCompleted{
				.exit_value = std::vector<Ref<vm::IVMValue>>{ &*seven, &*seven, &*forty_two },
			};

			auto values = extractPrimitiveValues(status);
			ASSERT_EQUAL_PRINT(3, values.size());
			ASSERT_EQUAL_PRINT("7", values[0]);
			ASSERT_EQUAL_PRINT("7", values[1]);
			ASSERT_EQUAL_PRINT("42", values[2]);
		}
	}

	void statusLineTest() {
		using vm::debugger::cli::common::statusLine;

		{ ASSERT_EQUAL_PRINT("New status: Running", statusLine(vm::api::Running{})); }

		{
			ASSERT_EQUAL_PRINT(
				"New status: ExecutionCompleted (return value = 7)",
				statusLine(vm::api::ExecutionCompleted{
					.exit_value = 7,
				})
			);
		}
	}

	void withoutControlSequencesTest() {
		using vm::debugger::cli::common::withoutControlSequences;

		ASSERT_EQUAL_PRINT(
			"Welcome to BeRD - an interactive in-DVM debugger! (now with replxx support!)",
			withoutControlSequences("\x1b[1mWelcome to \x1b[32mBeRD\x1b[0;1m - an interactive "
		                            "in-DVM debugger!\x1b[0m (now with replxx support!)")
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
