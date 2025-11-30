#include <diagnostic_interactive/core/term_ui_view.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

using namespace term_ui;
using namespace dia_int::term_ui_view;

#define CP(...) CodePiece(__VA_ARGS__)

#define SAMPLE_LOCATION "main.dmf", 15, 16

Message sample1() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage("value moved here", StyleType::Note));
	ptrs.put(1, PointerMessage("value used here after move", StyleType::Error));

	auto line_1 = CodeLine(19, { CP("let bob = have * a + dog;") });
	auto line_2 = CodeLine(
		20,
		{ CP("let that = "),
	      CP("is"),
	      CP("("),
	      CP("some", { 0 }),
	      CP(")"),
	      CP(" * example - "),
	      CP("code;", { 1 }) }
	);
	auto line_3 = CodeLine(
		21,
		{
			CP("let alice = have * a + cat;"),
		}
	);

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3 }, ptrs);

	Message m(StyleType::Error, 1'010, "alice, bob, and cat", {"", code});

	return m;
}

Message sample2() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage("first underline", StyleType::Warning));
	ptrs.put(1, PointerMessage("second underline", StyleType::Warning));
	ptrs.put(2, PointerMessage("third underline", StyleType::Warning));
	ptrs.put(3, PointerMessage("fourth underline", StyleType::Warning));
	ptrs.put(4, PointerMessage("fifth underline", StyleType::Warning));

	auto line_1 = CodeLine(
		9,
		{
			CP("this", { 4 }),
			CP(" is ", {2}),
			CP("some", { 1, 2, 3 }),
			CP(" ", { 1, 3 }),
			CP("text", { 1, 3 }),
			CP(" "),
			CP("there", { 0 }),
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
	ptrs.put(0, PointerMessage("message goes here", StyleType::Hint));

	auto line_1 = CodeLine(99, { CP("this is "), CP("only", { 0 }) });
	auto line_2 = CodeLine(100, { CP("a", { 0 }) });
	auto line_3 = CodeLine(101, { CP("shattered", { 0 }) });
	auto line_4 = CodeLine(102, { CP("sequence", { 0 }), CP(" that's all") });

	CodeSection code(SAMPLE_LOCATION, { line_1, line_2, line_3, line_4 }, ptrs);

	Message m(StyleType::Hint, 15, "hint here, hint there", {"", code});

	return m;
}

Message sample4() {
	auto ptrs = base::HashMap<u64, PointerMessage>();
	ptrs.put(0, PointerMessage("first underline", StyleType::Error));
	ptrs.put(1, PointerMessage("second underline", StyleType::Note));

	auto line_1
		= CodeLine(1'000, { CP("this ", { 1 }), CP("is some", { 0, 1 }), CP(" text", { 0 }) });

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
	ptrs.put(0, PointerMessage("first underline", StyleType::Docs));
	ptrs.put(1, PointerMessage("second underline", StyleType::Docs));
	ptrs.put(2, PointerMessage("third underline", StyleType::Hint));
	ptrs.put(30, PointerMessage("fourth underline", StyleType::Hint));

	auto line_1 = CodeLine(9'999'999, { 
		CP("a", { 0, 1 }), 
	CP(" "), 
	CP("a", { 2, 30 }) });
	auto line_2 = CodeLine(10'000'000, { CP("aaaa") });

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

int main() {
	print(sample1(), std::cerr);
	std::cerr << '\n';
	print(sample2(), std::cerr);
	std::cerr << '\n';
	print(sample3(), std::cerr);
	std::cerr << '\n';
	print(sample4(), std::cerr);
	std::cerr << '\n';
	print(sample5(), std::cerr);
	return 0;
}
