fun print_error(arg: bool) -> bool = {
    builtin_output_i64(99);
    return arg;
}

fun main() -> i64 = {

    var v: bool = true;
    if (v) {
        builtin_output_i64(1);                  # 1
    }
    v = not v;
    if (v) {
        builtin_output_i64(100);                # Should not execute
    }

    # Testing AND operator with variables
    v = true;
    if (v and v) {
        builtin_output_i64(2);                  # 2
    }
    if (v and true) {
        builtin_output_i64(3);                  # 3
    }
    if (true and v) {
        builtin_output_i64(4);                  # 4
    }
    if (v and false) {
        builtin_output_i64(101);                # Should not execute
    }
    if (false and v) {
        builtin_output_i64(102);                # Should not execute
    }

    v = not v;
    if (v and v) {
        builtin_output_i64(103);                # Should not execute
    }
    if (v and true) {
        builtin_output_i64(104);                # Should not execute
    }
    if (true and v) {
        builtin_output_i64(105);                # Should not execute
    }
    if (v and false) {
        builtin_output_i64(106);                # Should not execute
    }
    if (false and v) {
        builtin_output_i64(107);                # Should not execute
    }

    # Testing AND operator for laziness
    v = false;
    if (v and print_error(v)) {                 # Should not print
        builtin_output_i64(108);                # Should not execute
    }
    if (false and print_error(v)) {             # Should not print
        builtin_output_i64(109);                # Should not execute
    }

    # Testing OR operator with variables
    v = true;
    if (v or v) {
        builtin_output_i64(5);                  # 5
    }
    if (v or true) {
        builtin_output_i64(6);                  # 6
    }
    if (true or v) {
        builtin_output_i64(7);                  # 7
    }
    if (v or false) {
        builtin_output_i64(8);                  # 8
    }
    if (false or v) {
        builtin_output_i64(9);                  # 9
    }

    v = not v;
    if (v or v) {
        builtin_output_i64(110);                # Should not execute
    }
    if (v or true) {
        builtin_output_i64(10);                 # 10
    }
    if (true or v) {
        builtin_output_i64(11);                 # 11
    }
    if (v or false) {
        builtin_output_i64(111);                # Should not execute
    }
    if (false or v) {
        builtin_output_i64(112);                # Should not execute
    }

    # Testing OR for laziness
    v = true;
    if (v or print_error(true)) {               # Should not print
        builtin_output_i64(12);                 # 12
    }
    if (v or print_error(false)) {              # Should not print
        builtin_output_i64(13);                 # 13
    }


    # Testing NOT operator with variables
    v = false;
    if (not v) {
        builtin_output_i64(14);                 # 14
    }
    if (not (not v)) {
        builtin_output_i64(113);                # Should not execute
    }
    v = true;
    if (not v) {
        builtin_output_i64(114);                # Should not execute
    }
    if (not (not v)) {
        builtin_output_i64(15);                 # 15
    }


    # Testing XOR operator with variables
    v = true;
    if (v xor false) {
        builtin_output_i64(16);                 # 16
    }
    v = false;
    if (v xor false) {
        builtin_output_i64(115);                # Should not execute
    }

    # Testing laziness of and+or combo
    v = true;
    if (v or print_error(not v) and not v) {    # Should not print
        builtin_output_i64(17);                 # 17
    }
    if (v or not v and print_error(not v)) {    # Should not print
        builtin_output_i64(18);                 # 18
    }
    v = false;
    if (v and not v and print_error(not v)) {   # Should not print
        builtin_output_i64(116);                # Should not execute
    }
    if (v and print_error(not v) and not v) {   # Should not print
        builtin_output_i64(117);                # Should not execute
    }

    return 0;
}
