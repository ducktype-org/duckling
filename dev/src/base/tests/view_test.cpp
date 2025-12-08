#include <base/misc/raw_view.hpp>

#include <tester/tester.hpp>

class ViewTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ViewTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(rawViewTest);
		TESTER_ADD_TEST(owningViewTest);
	}

	void rawViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte* string = reinterpret_cast<const byte*>("Some random string");
		{ base::RawView view(string, 18); }
		base::RawView view(string, 18);

		assertTrue(view.stringView() == "Some random string", "bad RawView.stringView()");
		assertTrue(view.stdString() == "Some random string", "bad RawView.stdString()");
		assertTrue(view.getBegin() == string, "bad RawView.begin");
		assertTrue(view.size() == 18, "bad RawView.size");
	}

	void owningViewTest() {
		message("Parts of this test are relevant only under valgrind");

		const byte* string_1 = reinterpret_cast<const byte*>("Some random string 1");
		const char* string_2 = "Some random string 2";

		{
			base::OwningView empty_view_1;
			base::OwningView empty_view_2(nullptr);
		}

		{
			base::OwningView copy_view(string_2);
			assertTrue(
				copy_view.view().getBegin() != reinterpret_cast<const byte*>(string_2),
				"Owning view didn't make memory copy (1)"
			);
			assertTrue(copy_view.view().stringView() == string_2, "Owning view has bad content (1)");

			auto copy_view_2 = base::OwningView::copy(base::RawView(string_1, 20));
			assertTrue(
				copy_view_2.view().getBegin() != reinterpret_cast<const byte*>(string_1),
				"Owning view didn't make memory copy (2)"
			);
			assertTrue(
				copy_view_2.view().stringView() == reinterpret_cast<const char*>(string_1),
				"Owning view has bad content (2)"
			);
		}
	}

	~ViewTest() override = default;

private:
};

TESTER_COMMON_MAIN("/src/base/tests/");
