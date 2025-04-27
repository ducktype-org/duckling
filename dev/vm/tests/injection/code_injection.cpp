#include <tester/tester.hpp>

#include "vm/api/vm.hpp"
#include <vm/api/api.hpp>

class VmCodeInjectionTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmCodeInjectionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleFiles);
		TESTER_ADD_TEST(injectCode);
		TESTER_ADD_TEST(callVoidFunction);
		TESTER_ADD_TEST(callFunction);
	}


private:
	void multipleFiles() {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response->pid;

		fs::FilePath file1(path("multiple_files_1.dbc"));
		fs::FilePath file2(path("multiple_files_2.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file1, file2 }).has_value());

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		std::cerr << output_response->output << "\n";
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void injectCode() {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response->pid;

		fs::FilePath file1(path("inject_code_1.dbc"));
		auto         response1 = vm::api::loadFiles(pid, { file1 });

		fs::FilePath file2(path("inject_code_2.dbc"));
		auto response2 = vm::api::loadFiles(pid, { file2 });

		ASSERT_TRUE(vm::api::run(pid).has_value());
		ASSERT_TRUE(vm::api::input(pid, "123").has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		std::cerr << output_response->output << "\n";
		ASSERT_EQUAL("123", output_response->output);

		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void callVoidFunction() {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response->pid;

		fs::FilePath file(path("call_void_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer", {}).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		std::cerr << output_response->output << "\n";
		ASSERT_EQUAL("735", output_response->output);

		// TODO: This is something to thing about. What if functoins don't have an exit code?
		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}

	void callFunction() {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response->pid;

		fs::FilePath file(path("call_function.dbc"));
		ASSERT_TRUE(vm::api::loadFiles(pid, { file }).has_value());

		ASSERT_TRUE(vm::api::runFunction(pid, "summer", {1, 2}).has_value());
		ASSERT_TRUE(vm::api::join(pid).has_value());

		auto output_response = vm::api::output(pid);
		ASSERT_TRUE(output_response.has_value());
		std::cerr << output_response->output << "\n";
		ASSERT_EQUAL("3", output_response->output);

		// TODO: This is something to thing about. What if functoins don't have an exit code?
		auto exit_code_response = vm::api::getExitCode(pid);
		ASSERT_TRUE(exit_code_response.has_value());
		ASSERT_EQUAL_PRINT(0, *exit_code_response);
	}
};

TESTER_COMMON_MAIN("/vm/tests/injection/");
