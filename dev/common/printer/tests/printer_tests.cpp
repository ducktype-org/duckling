#include <tester/tester.hpp>
#include <printer/stream_printer.hpp>
#include <utility>
#include <sstream>

using namespace printer;

class PrinterTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PrinterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Printer test") { TESTER_ADD_TEST(test); }

private:
	// @TODO: add more tests.

	void test() {
		auto              mc1 = PrinterContent("ms1", Color::DEFAULT, Color::RED);
		std::stringstream ss1;
		printer::StreamPrinter::print(mc1, ss1);
		std::string expected1 = "\033[41mms1\033[0m";
		assert(
			ss1.str() == expected1,
			"Full print wrong output (" + ss1.str() + "), expected: (" + expected1 + ").",
			false
		);

		auto              mc2 = PrinterContent("ms2", Color::RED, Color::DEFAULT);
		std::stringstream ss2;
		printer::StreamPrinter::print({ mc1, mc2 }, ss2);
		std::string expected2 = "\033[41mms1\033[0m\033[31mms2\033[0m";
		assert(
			ss2.str() == expected2,
			"Full print wrong output (" + ss2.str() + "), expected (" + expected2 + ").",
			false
		);
	}
};

TESTER_COMMON_MAIN("/common/printer/tests/");
