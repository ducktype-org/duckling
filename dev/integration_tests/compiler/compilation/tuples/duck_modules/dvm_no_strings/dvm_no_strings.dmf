fun makeTuple(a: i64, b: i64) -> (i64, i64) = {
    return (a, b);
}

fun main() -> i64 = {
    var tuple = makeTuple(10, 20);
    builtin_output_i64(tuple._1);
    builtin_output_i64(tuple._2);

    tuple._1 = tuple._1 + 5;
    tuple._2 = tuple._1 + tuple._2;

    builtin_output_i64(tuple._1);
    builtin_output_i64(tuple._2);
    
    var tuple_2 = tuple;

    builtin_output_i64(tuple_2._1);
    builtin_output_i64(tuple_2._2);

    return 0;
}
