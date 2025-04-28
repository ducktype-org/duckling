#include <vm_tester_utils.hpp>

#include <vm/api/api.hpp>

class VmCodeInjectionTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCodeInjectionTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleFiles);
		TESTER_ADD_TEST(injectCode);
		TESTER_ADD_TEST(callNoArgFunction);
		TESTER_ADD_TEST(callVoidFunction);
		TESTER_ADD_TEST(callNonVoidFunction);
		TESTER_ADD_TEST(injectExistingFunction);
		// TESTER_ADD_TEST(repl);
	}

private:
	void multipleFiles() {
		vm::PID      pid = initProcess();
		fs::FilePath file1(path("multiple_files_1.dbc"));
		fs::FilePath file2(path("multiple_files_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1, file2 }).has_value());

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

	void callNoArgFunction() {
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

	void callVoidFunction() {
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

	void callNonVoidFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("call_non_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer", { 695, 40 }).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(735, *exit_code_response);
	}

	void injectExistingFunction() {
		vm::PID pid = initProcess();

		fs::FilePath file(path("inject_code_1.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());
		ASSERT_TRUE(!vm::api::loadFiles(pid, { file }).has_value());
	}
};

TESTER_COMMON_MAIN("/vm/tests/injection/");
