# Basic function
fun foo() -> i64 = {
    return 5i64 + 7i64;
}
 
fun main() -> i64 = {
    # Instantiating variables
    var a: i64 = 2i64;
    var b: i64 = 1i64;
    
    # Printing a literal
    builtin_output_i64(4i64);                # 4

    # Printing a local variable
    builtin_output_i64(b);                # 1

    # Printing an expression
    builtin_output_i64(foo() + b + 10i64);   # 23

    # Assigning a literal
    a = 4i64;

    builtin_output_i64(a);                # 4
    builtin_output_i64(b);                # 1

    # Assigning a local variable
    b = a;                       # b = 4

    builtin_output_i64(a); # 4
    builtin_output_i64(b); # 4

    # Assigning an expression
    a = ( a ) + ( b );           # a = 8

    builtin_output_i64(a);                # 8
    builtin_output_i64(b);                # 4


    # Assigning a more complex expression
    b = (2i64 * a) + 5i64 * (a + b); # b = 2 * 8 + 5 * 12 = 16 + 60 = 76

    builtin_output_i64(a);                # 8
    builtin_output_i64(b);                # 76

    return b;
}
