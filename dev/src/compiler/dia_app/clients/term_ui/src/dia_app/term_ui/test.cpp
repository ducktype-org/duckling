#include "view.hpp"

using namespace term_ui;

#define _C(...) CodePiece(__VA_ARGS__)

const CodeSection::Location sample_loc("main.dmf", 15, 16);

Info sample_1() {
	auto ptrs = base::HashMap<u32, PointerMessage>();
	ptrs.put(0, PointerMessage("value moved here", StyleType::Note));
	ptrs.put(1, PointerMessage("value used here after move", StyleType::Error));

	auto line_1 = CodeLine(19, { _C("let bob = have * a + dog;") });
	auto line_2 = CodeLine(
		20,
		{ _C("let that = "),
	      _C("is"),
	      _C("("),
	      _C("some", { 0 }),
	      _C(")"),
	      _C(" * example - "),
	      _C("code;", { 1 }) }
	);
	auto line_3 = CodeLine(
		21,
		{
			_C("let alice = have * a + cat;"),
		}
	);

	CodeSection code(sample_loc, { line_1, line_2, line_3 }, ptrs);

	Info m(StyleType::Error, 1'010, "alice, bob, and cat", "", code);

	return m;
}

Info sample_2() {
	auto ptrs = base::HashMap<u32, PointerMessage>();
	ptrs.put(0, PointerMessage("first underline", StyleType::Warning));
	ptrs.put(1, PointerMessage("second underline", StyleType::Warning));
	ptrs.put(2, PointerMessage("third underline", StyleType::Warning));
	ptrs.put(3, PointerMessage("fourth underline", StyleType::Warning));
	ptrs.put(4, PointerMessage("fifth underline", StyleType::Warning));

	auto line_1 = CodeLine(
		9,
		{
			_C("this", { 4 }),
			_C(" is "),
			_C("some", { 1, 2, 3 }),
			_C(" ", { 1, 3 }),
			_C("text", { 1, 3 }),
			_C(" "),
			_C("there", { 0 }),
		}
	);

	CodeSection code(sample_loc, { line_1 }, ptrs);

	Info m(
		StyleType::Warning,
		5'051,
		"that is quite some underlining",
		"The underlining strategy is complex and non-trivial. It may be used to convey additional "
	    "information.",
		code
	);

	return m;
}

Info sample_3() {
	auto ptrs = base::HashMap<u32, PointerMessage>();
	ptrs.put(0, PointerMessage("message goes here", StyleType::Hint));

	auto line_1 = CodeLine(99, { _C("this is "), _C("only", { 0 }) });
	auto line_2 = CodeLine(100, { _C("a", { 0 }) });
	auto line_3 = CodeLine(101, { _C("shattered", { 0 }) });
	auto line_4 = CodeLine(102, { _C("sequence", { 0 }), _C(" that's all") });

	CodeSection code(sample_loc, { line_1, line_2, line_3, line_4 }, ptrs);

	Info m(StyleType::Hint, 15, "hint here, hint there", "", code);

	return m;
}

Info sample_4() {
	auto ptrs = base::HashMap<u32, PointerMessage>();
	ptrs.put(0, PointerMessage("first underline", StyleType::Error));
	ptrs.put(1, PointerMessage("second underline", StyleType::Note));

	auto line_1
		= CodeLine(1'000, { _C("this ", { 1 }), _C("is some", { 0, 1 }), _C(" text", { 0 }) });

	CodeSection code(sample_loc, { line_1 }, ptrs);

	Info m(
		StyleType::Note,
		15,
		"this is a note, even though an error underlining is used in the code sample",
		"Do not do this in production. Error style is to be used only for errors.",
		code
	);

	return m;
}

Info sample_5() {
	auto ptrs = base::HashMap<u32, PointerMessage>();
	ptrs.put(0, PointerMessage("first underline", StyleType::Docs));
	ptrs.put(1, PointerMessage("second underline", StyleType::Docs));
	ptrs.put(2, PointerMessage("third underline", StyleType::Hint));
	ptrs.put(30, PointerMessage("fourth underline", StyleType::Hint));

	auto line_1 = CodeLine(9'999'999, { _C("a", { 0, 1 }), _C(" "), _C("a", { 2, 30 }) });
	auto line_2 = CodeLine(10'000'000, { _C("aaaa") });

	CodeSection code(sample_loc, { line_1, line_2 }, ptrs);

	Info m(
		StyleType::Docs,
		15,
		"docs color, nice one",
		"Again, do not use error underlining within non-error messages. This is for demonstration "
	    "purposes only.",
		code
	);

	return m;
}

int main() {
	sample_1().print(std::cerr);
	std::cerr << std::endl;
	sample_2().print(std::cerr);
	std::cerr << std::endl;
	sample_3().print(std::cerr);
	std::cerr << std::endl;
	sample_4().print(std::cerr);
	std::cerr << std::endl;
	sample_5().print(std::cerr);
}
