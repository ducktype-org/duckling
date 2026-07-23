var g1 = 1;
var g2 = 2;
var g3 = 3;
var g4 = 4;

fun step1() = {
  g1 = g1 + 1;
  g2 = g2 - 1;
  g3 = g3 * 1;
  g4 = g4 / 1;
}

fun step2() = {
  g1 = g1 + g2;
}

fun step3() = {
    if (g2 < g3 < g4) {
        g1 = g1 + 1;
    }
    else {
        g1 = g1 - 1;
    }
}

fun main() -> i64 = {
    step1(); # 2, 1, 3, 4
    step2(); # 3, 1, 3, 4
    step3(); # 4, 1, 3, 4
    builtin_output_i64(g1); 
    builtin_output_i64(g2); 
    builtin_output_i64(g3); 
    builtin_output_i64(g4); 
    return 0;
}
