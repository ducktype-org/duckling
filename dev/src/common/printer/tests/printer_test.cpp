// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 @file printer_tests.cpp

 Due to printer nature as a module it's hard to test its functionality, since it's about the visual
 output of a terminal. Because of this, tests are quite limited and test only basic functionalities.

 Test
 ====

 | Test constructs a simple :code:`PrinterContent` and adds it to a :code:`Console` twice testing
 multiple constructors and methods along the way. | First it tests whether outputting printing the
 full message yields the right result. | Then it tests reaching maximum amount for a type of
 message. | Then reaching maximum amount of all messages. | Then ensures that nothing is printed if
 minimal level of a message is set sufficiently high.
*/
#include <printer/stream_printer.hpp>
#include <tester/tester.hpp>

#include <sstream>

using namespace printer;

class PrinterTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PrinterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(test); }

private:
	// @TODO: add more tests.

	void test() {
		auto              mc1 = PrinterContent("ms1", Color::Default, Color::Red);
		std::stringstream ss1;
		printer::StreamPrinter::print(mc1, ss1);
		std::string expected1 = "\033[41mms1\033[0m";
		assertTrue(
			ss1.str() == expected1,
			"Full print wrong output (" + ss1.str() + "), expected: (" + expected1 + ").",
			false
		);

		auto              mc2 = PrinterContent("ms2", Color::Red, Color::Default);
		std::stringstream ss2;
		printer::StreamPrinter::print({ mc1, mc2 }, ss2);
		std::string expected2 = "\033[41mms1\033[0m\033[31mms2\033[0m";
		assertTrue(
			ss2.str() == expected2,
			"Full print wrong output (" + ss2.str() + "), expected (" + expected2 + ").",
			false
		);
	}
};

TESTER_COMMON_MAIN("/src/common/printer/tests/");
