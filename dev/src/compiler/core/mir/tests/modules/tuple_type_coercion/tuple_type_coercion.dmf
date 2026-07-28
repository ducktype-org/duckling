# Tuples whose target type has a `type` component. Every element that is not already a type has to
# be lifted to one while lowering, including elements which are plain values of unit type.

fun lift_tuple_elements() = {
    # A tuple literal, coerced element by element.
    var literal: (type, i64) = ((), 1);

    # A tuple that is already a value, so its elements are read back out of it before being lifted.
    var value: ((), i64)     = ((), 2);
    var lifted: (type, i64)  = value;

    # A nested tuple element, lifted as a whole.
    var nested: (type, type) = ((i32, i64), ());
}
