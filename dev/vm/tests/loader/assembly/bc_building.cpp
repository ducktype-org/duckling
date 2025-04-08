#include <vm_tester_utils.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm/bytecode/builders/errors.hpp>
#include <vm/loader/errors.hpp>
#include <vm/loader/parser/errors.hpp>
#include <vm/loader/parser/parser.hpp>

class BCBuildingTests: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS BCBuildingTests

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(multipleLabels);
		TESTER_ADD_TEST(repeatedTypes);
		TESTER_ADD_TEST(labelNotFound);
		TESTER_ADD_TEST(unknownType);
		TESTER_ADD_TEST(unknownFunction);
	}

private:
	/**
	 * @brief Preprocesses a syntactically incorrect file. Asserts that `error_keywords` are present
	 * in the error message.
	 */
	void buildInvalidDbc(
		const std::string& dbc_filename, const std::vector<std::string_view>& error_keywords
	) {
		auto process_pid_response = vm::api::spawn();
		ASSERT_TRUE(process_pid_response.has_value());
		auto pid = process_pid_response.expect("Spawn failed").pid;

		fs::FilePath file(path(dbc_filename));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		ASSERT_TRUE(!loaded_file_response.has_value());
		auto err = loaded_file_response.error();
		ASSERT_TRUE(std::holds_alternative<vm::api::CoreOperationError>(err));
		auto core_op = std::get<vm::api::CoreOperationError>(err);
		ASSERT_TRUE(std::holds_alternative<vm::api::LoadProgramError>(core_op));
		auto err_str = std::get<vm::api::LoadProgramError>(core_op).why;
		for (auto err_key: error_keywords) {
			assertTrue(
				err_str.find(err_key) != std::string::npos, base::strConcat("Not found: ", err_key)
			);
		}
	}

	void multipleLabels() {
		buildInvalidDbc(
			"multiple_labels.dbc",
			{
				vm::loader::RepeatedLabel::ERR_MSG,
				vm::loader::RepeatedLabelNote::ERR_MSG,
			}
		);
	}

	void repeatedTypes() {
		buildInvalidDbc(
			"repeated_types.dbc",
			{
				vm::loader::DuplicatedTypeError::ERR_MSG,
				vm::loader::DuplicatedTypeNote::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		buildInvalidDbc(
			"label_not_found.dbc",
			{
				vm::loader::UnknownLabel::ERR_MSG,
				"LB",
			}
		);
	}

	void unknownType() {
		buildInvalidDbc(
			"unknown_type.dbc",
			{
				base::strConcat(vm::loader::UnknownTypeError::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		buildInvalidDbc(
			"unknown_function.dbc",
			{
				base::strConcat(vm::loader::UnknownFunctionError::ERR_MSG, "foo"),
			}
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/loader/assembly/");
