// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <diagnostic/logger.hpp>
#include <diagnostic/source_position.hpp>
#include <tester/tester.hpp>
#include <token_parser_core/automatic.hpp>

#include <sstream>
#include <utility>

class ParsingErrorsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ParsingErrorsTest

	void diagnosticTests() {
		using dia::testDiagnosticMessage;
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
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(diagnosticTests); }

public:
	~ParsingErrorsTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/tokenizer/token_parser_core/");
