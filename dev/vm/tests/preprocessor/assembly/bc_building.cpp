#include <vm/code/builders/errors.hpp>
#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/load_program_error.hpp>
#include <vm_tester_utils.hpp>
#include <vm/preprocessor/parser/errors.hpp>
#include <vm/preprocessor/parser/parser.hpp>
#include <vm/preprocessor/errors.hpp>

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
				vm::preprocessor::RepeatedLabel::ERR_MSG,
				vm::preprocessor::RepeatedLabelNote::ERR_MSG,
			}
		);
	}

	void repeatedTypes() {
		buildInvalidDbc(
			"repeated_types.dbc",
			{
				vm::preprocessor::DuplicatedTypeError::ERR_MSG,
				vm::preprocessor::DuplicatedTypeNote::ERR_MSG,
			}
		);
	}

	void labelNotFound() {
		buildInvalidDbc(
			"label_not_found.dbc",
			{
				vm::preprocessor::UnknownLabel::ERR_MSG,
				"LB",
			}
		);
	}

	void unknownType() {
		buildInvalidDbc(
			"unknown_type.dbc",
			{
				base::strConcat(vm::preprocessor::UnknownType::ERR_MSG, "in64"),
			}
		);
	}

	void unknownFunction() {
		// @note: This is temporarily commented out until we fix function types
		// buildInvalidDbc(
		// 	"unknown_function.dbc",
		// 	{
		// 		base::strConcat(vm::preprocessor::UnknownFunction::ERR_MSG, "foo"),
		// 	}
		// );
		assertThrows<vm::code::builders::MissingFunctionalTypeError>(
			[&] { buildInvalidDbc("unknown_function.dbc", {}); }, "Missing func type not thrown"
		);
	}
};

TESTER_COMMON_MAIN("/vm/tests/preprocessor/assembly/");
