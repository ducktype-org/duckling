#include "message.hpp"

using namespace term_ui;

#define _C(...) CodePiece(__VA_ARGS__)

const CodeFragment::Location sample_loc("main.dmf", 15, 16);

Message sample_1() {
    auto ptrs = {
        PointerMessage("value moved here", StyleType::Note),
        PointerMessage("value used here after move", StyleType::Error)
    };

    auto line_1 = CodeLine(19, {_C("let bob = have * a + dog;")});
    auto line_2 = CodeLine(20, {
        _C("let that = "),
        _C("is"),
        _C("("),
        _C("some", {0}),
        _C(")"),
        _C(" * example - "),
        _C("code;", {1})
    });
    auto line_3 = CodeLine(21, {
        _C("let alice = have * a + cat;"),
    });
    
    CodeFragment code(sample_loc, {
        line_1,
        line_2,
        line_3
    }, ptrs);

    Message m(
        StyleType::Error,
        1010,
        "alice, bob, and cat",
        "",
        code
    );

    return m;
}

Message sample_2() {
    auto ptrs = {
        PointerMessage("first underline", StyleType::Warning),
        PointerMessage("second underline", StyleType::Warning),
        PointerMessage("third underline", StyleType::Warning),
        PointerMessage("fourth underline", StyleType::Warning),
        PointerMessage("fifth underline", StyleType::Warning)
    };

    auto line_1 = CodeLine(9, {
        _C("this", {4}),
        _C(" is "),
        _C("some", {1, 2, 3}),
        _C(" ", {1, 3}),
        _C("text", {1, 3}),
        _C(" "),
        _C("there", {0}),
    });

    CodeFragment code(sample_loc, {
        line_1
    }, ptrs);

    Message m(
        StyleType::Warning,
        5051,
        "that is quite some underlining",
        "The underlining strategy is complex and non-trivial. It may be used to convey additional information.",
        code
    );

    return m;
}

Message sample_3() {
    auto ptrs = {
        PointerMessage("message goes here", StyleType::Hint)
    };

    auto line_1 = CodeLine(99, {
        _C("this is "),
        _C("only", {0})
    });
    auto line_2 = CodeLine(100, {
        _C("a", {0})
    });
    auto line_3 = CodeLine(101, {
        _C("shattered", {0})
    });
    auto line_4 = CodeLine(102, {
        _C("sequence", {0}),
        _C(" that's all")
    });

    CodeFragment code(sample_loc, {
        line_1,
        line_2,
        line_3,
        line_4
    }, ptrs);

    Message m(
        StyleType::Hint,
        15,
        "hint here, hint there",
        "",
        code
    );

    return m;
}

Message sample_4() {
    auto ptrs = {
        PointerMessage("first underline", StyleType::Error),
        PointerMessage("second underline", StyleType::Note)
    };

    auto line_1 = CodeLine(1000, {
        _C("this ", {1}),
        _C("is some", {0, 1}),
        _C(" text", {0})
    });

    CodeFragment code(sample_loc, {
        line_1
    }, ptrs);

    Message m(
        StyleType::Note,
        15,
        "this is a note, even though an error underlining is used in the code sample",
        "Do not do this in production. Error style is to be used only for errors.",
        code
    );

    return m;
}

Message sample_5() {
    auto ptrs = {
        PointerMessage("first underline", StyleType::Error),
        PointerMessage("second underline", StyleType::Error),
        PointerMessage("third underline", StyleType::Note),
        PointerMessage("fourth underline", StyleType::Note)
    };

    auto line_1 = CodeLine(9999999, {
        _C("a", {0, 1}),
        _C(" "),
        _C("a", {2, 3})
    });
    auto line_2 = CodeLine(10000000, {
        _C("aaaa")
    });

    CodeFragment code(sample_loc, {
        line_1,
        line_2
    }, ptrs);

    Message m(
        StyleType::Docs,
        15,
        "docs color, nice one",
        "Again, do not use error underlining within non-error messages. This is for demonstration purposes only.",
        code
    );

    return m;
}

int main() {

    sample_1().print();
    std::cout << std::endl;
    sample_2().print();
    std::cout << std::endl;
    sample_3().print();
    std::cout << std::endl;
    sample_4().print();
    std::cout << std::endl;
    sample_5().print();
}