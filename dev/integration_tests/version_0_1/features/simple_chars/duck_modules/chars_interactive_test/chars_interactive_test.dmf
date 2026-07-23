# Basic character arithmetic tests

fun test_char_comparisons() = {
    let a: char = 'a';
    let b: char = 'b';

    let lt: bool = a < b;
    let le: bool = a <= b;
    let gt: bool = a > b;
    let ge: bool = a >= b;
    let eq: bool = a == b;
    let neq: bool = a != b;

    if (not lt or not le or gt or ge or eq or not neq) {
        print("bad 1");
    }
}

fun test_char_difference() = {
    let a: char = 'a';
    let d: char = 'd';

    let diff: u8 = d - a;  # 3
    if (diff != 3) {
        print("bad 2");
    }
}

fun test_char_int_addition() = {
    let a: char = 'a';

    # Character + integer should return char
    let d1: char = a + 3u8;        # 'd'
    let d2: char = 3u8 + a;        # 'd'

    if (d1 != 'd' or d2 != 'd') {
        print("bad 3");
    }
}

# Something more fun.

fun process_character() -> () = {
    print("Enter a character: ");
    var c = builtin_input_char();

    if ('a' <= c <= 'z') {
        print("The provided character is lowercase.\n");
        print("Its uppercase counterpart is: ");
        builtin_output_char(c - 'a' + 'A');
        print("\n");
    } else if ('A' <= c <= 'Z') {
        print("The provided character is uppercase.\n");
        print("Its lowercase counterpart is: ");
        builtin_output_char(c - 'A' + 'a');
        print("\n");
    } else {
        print("The provided character is not a letter.\n");
    }
}

fun main() -> i64 = {
    # Start with the basic tests
    test_char_comparisons();
    test_char_difference();
    test_char_int_addition();

    # Now do something interesting.
    var reps = 3;
    while (reps > 0) {
        process_character();
        reps = reps - 1;
    }
    return 0;
}
