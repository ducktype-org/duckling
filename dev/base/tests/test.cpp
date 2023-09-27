#include <base/defer.hpp>
#include <base/raw_view.hpp>
#include <base/strongly_typed_int.hpp>
#include <cstring>
#include <tester/tester.hpp>
#include <tester/testing_utils.hpp>

STRONG_TYPEDEF_INT_DIMENSIONAL(Meters, i64);

bool compareCstr(const char *const c1, const char *const c2) {
	return std::string_view(c1) == std::string_view(c2);
}

bool containsCstr(const char *const base, const char *const pattern) {
	return std::strstr(base, pattern) != NULL;
}

class GeneralUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS GeneralUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("General base Test") {
		TESTER_ADD_TEST(testPanic1);
		TESTER_ADD_TEST(testPanic2);
		TESTER_ADD_TEST(testPanic3);
		TESTER_ADD_TEST(rawViewTest);
		TESTER_ADD_TEST(owningViewTest);
		TESTER_ADD_TEST(verySimpleTestingUtilsTest);
		TESTER_ADD_TEST(stronglyTypedInt);
		TESTER_ADD_TEST(deferTest);
		TESTER_ADD_TEST(testStrConcat);
	}

private:
	void testStrConcat() {
		std::string res;

		res = base::strConcat("aBd", 1, 5, true, "Inny string");
		assert(res == "aBd15trueInny string", "strConcta returned answer other than expected");

		res = base::strConcat();
		assert(res == "", "strConcta returned answer other than expected");

		res = base::strConcat("", "", "");
		assert(res == "", "strConcta returned answer other than expected");

		res = base::strConcat(1, -87, 123'456'789ull);
		assert(res == "1-87123456789", "strConcta returned answer other than expected");

		// res = base::strConcat("abacabadaba", nullptr);
		// assert(res == "", "strConcta returned answer other than expected");
	}

	void throwPanic1() { throw base::Panic("throwPanic", "panic test"); }

	void throwPanic2() { RIFT_PANIC("panic test 2"); }

	void throwPanic3() { RIFT_ASSERT(false, "panic test 3"); }

	void testPanic1() {
		try {
			throwPanic1();
		} catch (base::Panic &panic) {
			assert(panic.getPosition() == "throwPanic", "Bad panic position");
			assert(containsCstr(panic.what(), "panic test"), "Bad panic reason");
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic2() {
		try {
			throwPanic2();
		} catch (base::Panic &panic) {
			assert(
				containsCstr(panic.what(), "    Panic thrown:\n    panic test 2"),
				"Bad panic reason"
			);
			return;
		}
		fail("Panic what not caught");
	}

	void testPanic3() {
		try {
			throwPanic3();
		} catch (base::Panic &panic) {
			assert(
				containsCstr(panic.what(), "    Assertion failed: `false`\n    panic test 3"),
				"Bad panic reason"
			);
			return;
		}
		fail("Panic what not caught");
	}

	void testNotYetImplemented() {
		try {
			throw base::NotYetImplemented("NotYetImplemented test");
		} catch (base::NotYetImplemented &nyi) {
			assert(
				compareCstr(nyi.what(), "NotYetImplemented test"), "Bad NotYetImplemented reason"
			);
			return;
		}
		fail("NotYetImplemented what not caught");
	}

	void testLogicError() {
		try {
			throw base::LogicError("Logic error test");
		} catch (base::LogicError &le) {
			assert(compareCstr(le.what(), "Logic error test"), "Bad LogicError reason");
			return;
		}
		fail("LogicError what not caught");
	}

	void rawViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte *string = reinterpret_cast<const byte *>("Some random string");
		{ base::RawView view(string, 18); }
		base::RawView view(string, 18);

		assert(view.stringView() == "Some random string", "bad RawView.stringView()");
		assert(view.stdString() == "Some random string", "bad RawView.stdString()");
		assert(view.getBegin() == string, "bad RawView.begin");
		assert(view.size() == 18, "bad RawView.size");
	}

	void owningViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte *string_1 = reinterpret_cast<const byte *>("Some random string 1");
		const char *string_2 = "Some random string 2";

		{
			base::OwningView empty_view_1;
			base::OwningView empty_view_2(nullptr);
		}

		{
			base::OwningView copy_view(string_2);
			assert(
				copy_view.view().getBegin() != reinterpret_cast<const byte *>(string_2),
				"Owning view didn't make memory copy (1)"
			);
			assert(copy_view.view().stringView() == string_2, "Owning view has bad content (1)");

			auto copy_view_2 = base::OwningView::copy(base::RawView(string_1, 20));
			assert(
				copy_view_2.view().getBegin() != reinterpret_cast<const byte *>(string_1),
				"Owning view didn't make memory copy (2)"
			);
			assert(
				copy_view_2.view().stringView() == reinterpret_cast<const char *>(string_1),
				"Owning view has bad content (2)"
			);
		}
	}

	void verySimpleTestingUtilsTest() {
		assert(testing_utils::compareJson(" {}", "{ }"), "Incorrect compareJson (1)");

		assert(
			testing_utils::compareJson(R"--( { "data" : {} })--", R"--(  { "data" : {  } } )--"),
			"Incorrect compareJson (2)"
		);

		assert(
			!testing_utils::compareJson(R"--( { "data" : [] })--", R"--(  { "data" : {  } } )--"),
			"Incorrect compareJson (3)"
		);

		assert(
			!testing_utils::compareJson(
				R"--( { "data" :  { }, "data2" : {} })--", R"--(  { "data" : {  } } )--"
			),
			"Incorrect compareJson (4)"
		);
	}

	void stronglyTypedInt() {
		Meters m(0);
		assert(i64(m) == 0, "Basic math failed (1)");
		assert(m == Meters(0), "Basic math failed (2)");

		Meters m1(2);
		Meters m2 = m + m1;
		assert(m2 == Meters(2), "Basic math failed (3)");
		assert(-m2 == Meters(-2), "Basic math failed (4)");
		assert(+(-m2) == Meters(-2), "Basic math failed (5)");
		assert(m2 != Meters(1), "Basic math failed (6)");
		assert(m2 > Meters(0), "Basic math failed (7)");
		assert(m2 > Meters(1), "Basic math failed (8)");
		assert(m2 >= Meters(2), "Basic math failed (9)");
		assert(m2 <= Meters(2), "Basic math failed (10)");
		assert(m2 < Meters(3), "Basic math failed (11)");
		assert(m2 < Meters(10), "Basic math failed (12)");

		assert(m2 - Meters(10) == Meters(-8), "Basic math failed (13)");
		assert(m2 * 2 == Meters(4), "Basic math failed (14)");
		assert(m2 / 2 == Meters(1), "Basic math failed (15)");

		m2 += Meters(10);
		assert(m2 == Meters(12), "Basic math failed (16)");
		m2 -= Meters(20);
		assert(m2 == Meters(-8), "Basic math failed (17)");
	}

	void deferTest() {
		i32 a = 0;
		{ defer(a = 1); }
		assert(a == 1, "Defer didn't execute or didn't capture variable");

		i32 b = 0;
		i32 c = 0;
		{
			c = 100;
			defer({
				b = 1;
				c = 2;
			});
			c = 100;
		}
		assert(b == 1, "Defer didn't execute after all other statements (1)");
		assert(c == 2, "Defer didn't execute after all other statements (2)");

		{
			defer(a = 3);
			defer(a = 2);
			a = 4;
		}
		assert(a == 3, "Defer didn't execute in correct order");
	}
};

TESTER_COMMON_MAIN("/base/tests/");
