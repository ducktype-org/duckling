#include <token_parser_core/automatic.hpp>

#include <diagnostic/source_position.hpp>
#include <diagnostic_interactive/logger.hpp>
#include <tester/tester.hpp>

#include <sstream>
#include <utility>

class ParsingErrorsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ParsingErrorsTest

	
	void diagnosticTests() {
		using dia_int::testDiagnosticMessage;
		std::stringstream ss;

        testDiagnosticMessage<tpc::NoIdentifierError>(
			ss, dia::SourcePosition::fakePosition(), "DUMMY TOKEN"
		);

        testDiagnosticMessage<tpc::NoKeywordError>(
			ss, dia::SourcePosition::fakePosition(), "DUMMY TOKEN"
		);

		std::cerr << ss.str();
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(diagnosticTests);
	}

public:
	~ParsingErrorsTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/tokenizer/token_parser_core/");
