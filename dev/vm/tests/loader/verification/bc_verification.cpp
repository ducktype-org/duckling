#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/loader/errors.hpp>

#include <variant>

class BCVerificationTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCVerificationTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(noMain);
		TESTER_ADD_TEST(invalidMainRetSize);
		TESTER_ADD_TEST(invalidMainRetType);
	}

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
		auto pid = process_pid_response.expect("Spawn failed").pid;

		fs::FilePath file(path(dbc_filename));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		ASSERT_TRUE(loaded_file_response.has_error());
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

	void noMain() { parseInvalidDbc("wrong/functions/no_main.dbc", { vm::loader::NO_MAIN_ERR }); }

	void invalidMainRetSize() {
		parseInvalidDbc(
			"wrong/functions/invalid_main_ret_size.dbc", { vm::loader::WRONG_MAIN_RET_VAL_ERR }
		);
	}

	void invalidMainRetType() {
		parseInvalidDbc(
			"wrong/functions/invalid_main_ret_type.dbc", { vm::loader::WRONG_MAIN_RET_VAL_ERR }
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/loader/verification/");
