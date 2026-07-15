
template(T: type)
fun countDown(n: T) = {
    builtin_output_i64(n);
    if (n == 0) return 0;

    # @TODO: #3112 consider what to do with it,
    # it does not currently work, because
    # both inserted countDown symbol and original
    # template symbols are found in lookup. 
    return countDown(n - 1);
}

fun main() -> i64 = {
    countDown:{i64}(3);
    return 0;
}
