#include <vm_tester_utils.hpp>

#include "vm/api/data/core_operation_error.hpp"
#include <vm/api/api.hpp>

class VmCodeInjectionTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCodeInjectionTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleFiles);
		TESTER_ADD_TEST(injectCode);
		TESTER_ADD_TEST(runNoArgFunction);
		TESTER_ADD_TEST(runVoidFunction);
		TESTER_ADD_TEST(runNonVoidFunction);
		TESTER_ADD_TEST(doubleRunFunction);
		TESTER_ADD_TEST(manyRunFunctions);
		TESTER_ADD_TEST(repl);
		TESTER_ADD_TEST(injectExistingFunction);
	}

private:
	void multipleFiles() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("multiple_files_1.dbc"));
		fs::FilePath file2(path("multiple_files_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1, file2 }).has_value());

		// auto loaded_file_response = vm::api::loadFiles(pid, { file1, file2 });
		// if (!loaded_file_response.has_value()) {
		// 	auto err = loaded_file_response.error();
		// 	ASSERT_TRUE(std::holds_alternative<vm::api::CoreOperationError>(err));
		// 	auto core_op = std::get<vm::api::CoreOperationError>(err);
		// 	ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(core_op));
		// 	auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
		// 	std::cerr << err_str << '\n';
		// }

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void injectCode() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("inject_code_1.dbc"));
		fs::FilePath file2(path("inject_code_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void runNoArgFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("call_no_arg_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("735", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void runVoidFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("call_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer", { 1, 2 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		ASSERT_EQUAL("3", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void runNonVoidFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("call_non_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer", { 695, 40 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(735, *exit_code_response);
	}

	void doubleRunFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::runFunction(pid, "spring", { 4, 8 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(32, *exit_code_response);

		ASSERT_TRUE(vm::api::runFunction(pid, "spring", { 4, 6 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(24, *exit_code_response);
	}

	void manyRunFunctions() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		for (i32 i = 0; i < 100; i++) {
			ASSERT_TRUE(vm::api::runFunction(pid, "spring", { i, i }).has_value());
			ASSERT_TRUE(vm::api::join(pid).has_value());

			auto exit_code_response = vm::api::getExitCode(pid);
			ASSERT_TRUE(exit_code_response.has_value());
			ASSERT_EQUAL_PRINT(i*i, *exit_code_response);
		}
	}

	void repl() {
		vm::PID pid = initProcess();

		fs::FilePath file1(path("repl_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1 }).has_value());
		ASSERT_TRUE(vm::api::runFunction(pid, "spring", { 4, 8 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(32, *exit_code_response);

		fs::FilePath file2(path("repl_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file2 }).has_value());
		ASSERT_TRUE(vm::api::runFunction(pid, "summer", { 1, 2 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(3, *exit_code_response);
	}

	void injectExistingFunction() {
		vm::PID      pid = initProcess();
		fs::FilePath file(path("inject_code_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		ASSERT_TRUE(!vm::api::loadFiles(pid, { file }).has_value());
	}
};

TESTER_COMMON_MAIN("/vm/tests/injection/");
