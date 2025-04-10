#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/loader/validator/errors.hpp>

#include <variant>

class BCValidationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCValidationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(noMain); }

private:
	/**
	 * @brief Parses a file containing a program which violates static verification guidelines.
	 * Asserts that `error_keywords` are present in the error message.
	 */
	void parseInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	) {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response.value().pid; // "Spawn failed"

		fs::FilePath file(path(dbc_filename));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		ASSERT_TRUE(!loaded_file_response.has_value());
		auto err = loaded_file_response.error();
		ASSERT_TRUE(std::holds_alternative<vm::api::CoreOperationError>(err));
		auto core_op = std::get<vm::api::CoreOperationError>(err);
		ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(core_op));
		auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
		// std::cerr << err_str << '\n';
		for (auto err_key: error_keywords) {
			assertTrue(
				err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
			);
		}
	}

	void noMain() { parseInvalidDbc("no_main.dbc", { vm::loader::validator::NO_MAIN_ERR }); }
};

TESTER_COMMON_MAIN("/vm/tests/loader/validation/");
