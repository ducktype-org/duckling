#include <diagnostic_interactive/core/term_ui_view.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

#include <tester/tester.hpp>

using namespace term_ui;
using namespace dia_int::term_ui_view;

// NOLINTBEGIN(missing-field-initializers)

constexpr dia_int::CodeLocation SAMPLE_LOCATION{
	.file = "main.dmf", .line = 15, .column = 16, .end_line = {}, .end_column = {}
};

Message sample1() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(
		0, PointerMessage{ .text = "value moved here", .type = StyleType::Note, .priority = 0 }
	);
	ptrs.put(
		1,
		PointerMessage{
			.text = "value used here after move", .type = StyleType::Error, .priority = 0 }
	);

	auto line_1
		= CodeLine(19, { CodePiece{ .text = "let bob = have * a + dog;", .pointer_ids = {} } });
	auto line_2 = CodeLine(
		20,
		{ CodePiece{ .text = "let that = ", .pointer_ids = {} },
	      CodePiece{ .text = "is", .pointer_ids = {} },
	      CodePiece{ .text = "(", .pointer_ids = {} },
	      CodePiece{ .text = "some", .pointer_ids = { 0 } },
	      CodePiece{ .text = ")", .pointer_ids = {} },
	      CodePiece{ .text = " * example - ", .pointer_ids = {} },
	      CodePiece{ .text = "code;", .pointer_ids = { 1 } } }
	);
	auto line_3 = CodeLine(
		21,
		{
			CodePiece{ .text = "let alice = have * a + cat;", .pointer_ids = {} },
		}
	);

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3 }, ptrs);

	Message m(StyleType::Error, 1'010, "alice, bob, and cat", { "", code });

	return m;
}

Message sample2() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(
		0, PointerMessage{ .text = "first underline", .type = StyleType::Warning, .priority = 0 }
	);
	ptrs.put(
		1, PointerMessage{ .text = "second underline", .type = StyleType::Warning, .priority = 0 }
	);
	ptrs.put(
		2, PointerMessage{ .text = "third underline", .type = StyleType::Warning, .priority = 0 }
	);
	ptrs.put(
		3, PointerMessage{ .text = "fourth underline", .type = StyleType::Warning, .priority = 0 }
	);
	ptrs.put(
		4, PointerMessage{ .text = "fifth underline", .type = StyleType::Warning, .priority = 0 }
	);

	auto line_1 = CodeLine(
		9,
		{
			CodePiece{ .text = "this", .pointer_ids = { 4 } },
			CodePiece{ .text = " is ", .pointer_ids = { 2 } },
			CodePiece{ .text = "some", .pointer_ids = { 1, 2, 3 } },
			CodePiece{ .text = " ", .pointer_ids = { 1, 3 } },
			CodePiece{ .text = "text", .pointer_ids = { 1, 3 } },
			CodePiece{ .text = " ", .pointer_ids = {} },
			CodePiece{ .text = "there", .pointer_ids = { 0 } },
		}
	);

	CodeSection code(SAMPLE_LOCATION, { line_1 }, ptrs);

	Message m(
		StyleType::Warning,
		5'051,
		"that is quite some underlining",
		{ "The underlining strategy is complex and non-trivial. It may be used to convey "
	      "additional "
	      "information.",
	      code }
	);

	return m;
}

Message sample3() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(
		0, PointerMessage{ .text = "message goes here", .type = StyleType::Hint, .priority = 0 }
	);

	auto line_1 = CodeLine(
		99,
		{ CodePiece{ .text = "this is ", .pointer_ids = {} },
	      CodePiece{ .text = "only", .pointer_ids = { 0 } } }
	);
	auto line_2 = CodeLine(100, { CodePiece{ .text = "a", .pointer_ids = { 0 } } });
	auto line_3 = CodeLine(101, { CodePiece{ .text = "shattered", .pointer_ids = { 0 } } });
	auto line_4 = CodeLine(
		102,
		{ CodePiece{ .text = "sequence", .pointer_ids = { 0 } },
	      CodePiece{ .text = " that's all", .pointer_ids = {} } }
	);

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3, line_4 }, ptrs);

	Message m(StyleType::Hint, 15, "hint here, hint there", { "", code });

	return m;
}

Message sample4() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(
		0, PointerMessage{ .text = "first underline", .type = StyleType::Error, .priority = 0 }
	);
	ptrs.put(
		1, PointerMessage{ .text = "second underline", .type = StyleType::Note, .priority = 0 }
	);

	auto line_1 = CodeLine(
		1'000,
		{ CodePiece{ .text = "this ", .pointer_ids = { 1 } },
	      CodePiece{ .text = "is some", .pointer_ids = { 0, 1 } },
	      CodePiece{ .text = " text", .pointer_ids = { 0 } } }
	);

	CodeSection code(SAMPLE_LOCATION, { line_1 }, ptrs);

	Message m(
		StyleType::Note,
		15,
		"this is a note, even though an error underlining is used in the code sample",
		{ "Do not do this in production. Error style is to be used only for errors.", code }
	);

	return m;
}

Message sample5() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage{ .text = "first underline", .type = StyleType::Docs, .priority = 0 });
	ptrs.put(
		1, PointerMessage{ .text = "second underline", .type = StyleType::Docs, .priority = 0 }
	);
	ptrs.put(2, PointerMessage{ .text = "third underline", .type = StyleType::Hint, .priority = 0 });
	ptrs.put(
		30, PointerMessage{ .text = "fourth underline", .type = StyleType::Hint, .priority = 0 }
	);

	auto line_1 = CodeLine(
		9'999'999,
		{ CodePiece{ .text = "a", .pointer_ids = { 0, 1 } },
	      CodePiece{ .text = " ", .pointer_ids = {} },
	      CodePiece{ .text = "a", .pointer_ids = { 2, 30 } } }
	);
	auto line_2 = CodeLine(10'000'000, { CodePiece{ .text = "aaaa", .pointer_ids = {} } });

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2 }, ptrs);

	Message m(
		StyleType::Docs,
		15,
		"docs color, nice one",
		{ "Again, do not use error underlining within non-error messages. This is for "
	      "demonstration "
	      "purposes only.",
	      code }
	);

	return m;
}

// NOLINTEND(missing-field-initializers)


class TermUITester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TermUITester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(mainTest); }

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
