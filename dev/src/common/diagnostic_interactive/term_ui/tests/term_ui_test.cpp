#include <diagnostic_interactive/core/term_ui_view.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>
#include <tester/tester.hpp>

using namespace term_ui;
using namespace dia_int::term_ui_view;

// NOLINTBEGIN(missing-field-initializers)

#define SAMPLE_LOCATION "main.dmf", 15, 16


Message sample1() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{"value moved here", StyleType::Note, 0});
	ptrs.put(1, PointerMessage{"value used here after move", StyleType::Error, 0});

	auto line_1 = CodeLine(19, { CodePiece{"let bob = have * a + dog;", {}} });
	auto line_2 = CodeLine(
		20,
		{ CodePiece{"let that = ", {}},
	      CodePiece{"is", {}},
	      CodePiece{"(", {}},
	      CodePiece{"some", { 0 }},
	      CodePiece{")", {}},
	      CodePiece{" * example - ", {}},
	      CodePiece{"code;", { 1 }} }
	);
	auto line_3 = CodeLine(
		21,
		{
			CodePiece{"let alice = have * a + cat;", {}},
		}
	);

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3 }, ptrs);

	Message m(StyleType::Error, 1'010, "alice, bob, and cat", {"", code});

	return m;
}

Message sample2() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{"first underline", StyleType::Warning, 0});
	ptrs.put(1, PointerMessage{"second underline", StyleType::Warning, 0});
	ptrs.put(2, PointerMessage{"third underline", StyleType::Warning, 0});
	ptrs.put(3, PointerMessage{"fourth underline", StyleType::Warning, 0});
	ptrs.put(4, PointerMessage{"fifth underline", StyleType::Warning, 0});

	auto line_1 = CodeLine(
		9,
		{
			CodePiece{"this", { 4 }},
			CodePiece{" is ", {2}},
			CodePiece{"some", { 1, 2, 3 }},
			CodePiece{" ", { 1, 3 }},
			CodePiece{"text", { 1, 3 }},
			CodePiece{" ", {}},
			CodePiece{"there", { 0 }},
		}
	);

	CodeSection code(SAMPLE_LOCATION, { line_1 }, ptrs);

	Message m(
		StyleType::Warning,
		5'051,
		"that is quite some underlining",
		{"The underlining strategy is complex and non-trivial. It may be used to convey additional "
		"information.",
		code}
	);

	return m;
}

Message sample3() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{"message goes here", StyleType::Hint, 0});

	auto line_1 = CodeLine(99, { CodePiece{"this is ", {}}, CodePiece{"only", { 0 }} });
	auto line_2 = CodeLine(100, { CodePiece{"a", { 0 }} });
	auto line_3 = CodeLine(101, { CodePiece{"shattered", { 0 }} });
	auto line_4 = CodeLine(102, { CodePiece{"sequence", { 0 }}, CodePiece{" that's all", {}} });

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3, line_4 }, ptrs);

	Message m(StyleType::Hint, 15, "hint here, hint there", {"", code});

	return m;
}

Message sample4() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{"first underline", StyleType::Error, 0});
	ptrs.put(1, PointerMessage{"second underline", StyleType::Note, 0});

	auto line_1
		= CodeLine(1'000, { CodePiece{"this ", { 1 }}, CodePiece{"is some", { 0, 1 }}, CodePiece{" text", { 0 }} });

	CodeSection code(SAMPLE_LOCATION, { line_1 }, ptrs);

	Message m(
		StyleType::Note,
		15,
		"this is a note, even though an error underlining is used in the code sample",
		{"Do not do this in production. Error style is to be used only for errors.",
		code}
	);

	return m;
}

Message sample5() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{"first underline", StyleType::Docs, 0});
	ptrs.put(1, PointerMessage{"second underline", StyleType::Docs, 0});
	ptrs.put(2, PointerMessage{"third underline", StyleType::Hint, 0});
	ptrs.put(30, PointerMessage{"fourth underline", StyleType::Hint, 0});

	auto line_1 = CodeLine(9'999'999, { 
		CodePiece{"a", { 0, 1 }}, 
	CodePiece{" ", {}}, 
	CodePiece{"a", { 2, 30 }} });
	auto line_2 = CodeLine(10'000'000, { CodePiece{"aaaa", {}} });

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2 }, ptrs);

	Message m(
		StyleType::Docs,
		15,
		"docs color, nice one",
		{"Again, do not use error underlining within non-error messages. This is for demonstration "
		"purposes only.",
		code}
	);

	return m;
}

// NOLINTEND(missing-field-initializers)


class TermUITester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TermUITester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(mainTest);
	}

private:
	void mainTest() {

		print(sample1(), std::cerr);
		std::cerr << '\n';
		print(sample2(), std::cerr);
		std::cerr << '\n';
		print(sample3(), std::cerr);
		std::cerr << '\n';
		print(sample4(), std::cerr);
		std::cerr << '\n';
		print(sample5(), std::cerr);
	}
};
TESTER_COMMON_MAIN("/src/common/diagnostic_interactive/term_ui/tests/");
