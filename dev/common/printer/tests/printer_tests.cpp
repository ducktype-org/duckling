#include <printer/printer.hpp>
#include <sstream>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>
#include <utility>

using namespace printer;

class PrinterTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS PrinterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Printer test") { TESTER_ADD_TEST(test); }

private:
	// @TODO: add more tests.

	void test() {
		MessageContent mc = MessageContent("ms1", Color::DEFAULT, Color::RED);

		Message     m({ mc });
		MessagePack mp = { m };
		Console     c;
		c.add(mp);
		c.add(std::move(mp));

		c.setMaxAmounts(MessageType::ERROR, 0);
		c.setMinLevel(MessageType::ERROR, 1);
		std::stringstream ss1;
		c.print(ss1);
		assert(ss1.str() == "\033[0m\033[41mms1\033[0m\n\033[0m\033[41mms1\033[0m\n",
		       "Full print wrong output (" + ss1.str() + ").",
		       false);

		c.setMaxAmounts(MessageType::ALL, 0);
		std::stringstream ss2;
		c.print(ss2);
		assert(ss2.str() == "Limit for this type of message has been reached.\n",
		       "Message type limit print wrong output (" + ss2.str() + ").",
		       false);

		c.setGeneralMax(0);
		std::stringstream ss3;
		c.print(ss3);
		assert(ss3.str() == "Limit for messages has been reached.\n",
		       "General limit print wrong output (" + ss3.str() + ").",
		       false);

		c.setMinLevel(MessageType::ALL, 1);
		std::stringstream ss4;
		c.print(ss4);
		assert(ss4.str() == "", "Min level print wrong output (" + ss4.str() + ").", false);
	}
};

TESTER_COMMON_MAIN("/common/printer/tests/");
