#include <api/api.hpp>
#include <tester/tester.hpp>

class StaticTableVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StaticTableVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Static Table VM Test") {
		TESTER_ADD_TEST(oneValue);
		TESTER_ADD_TEST(arrSum);
		TESTER_ADD_TEST(initWithZero);
	}

private:
	void runTestOnVm(
		const std::string& rbc_filename, const std::string& input, const std::string& output
	) {
		auto process_pid_response = vm::api::spawn(false);
		assert(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		fs::FilePath file(path(rbc_filename));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assert(loaded_file_response.has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assert(run_response.has_value(), "Run failed (1)");

		auto input_response = vm::api::input(pid, input);
		assert(input_response.has_value(), "Input failed (1)");

		auto output_response = vm::api::output(pid);
		assert(output_response.has_value(), "Output failed (1)");
		std::cerr << output_response.value().output << std::endl;
		assert(output_response.value().output == output, "Output failed (2)");

		auto join_response = vm::api::join(pid);
		assert(join_response.has_value(), "Join failed (1)");
	}

	void oneValue() { runTestOnVm("one_value.rbc", "", "0"); }

	void arrSum() { runTestOnVm("arr_sum.rbc", "", "903"); }

	void initWithZero() { runTestOnVm("init_with_zero.rbc", "", "0"); }
};

TESTER_COMMON_MAIN("/RiftVM/tests/static_table/");
