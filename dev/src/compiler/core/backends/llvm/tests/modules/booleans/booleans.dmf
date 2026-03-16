fun foo() = {
    if (true) {

    }
    if (false) {

    }
}

fun goo() = {
    let a1 : bool = true;
    let a2 : bool = false;

    let a31 : bool = a1 and true;
    let a32 : bool = a1 and false;
    let a41 : bool = a1 or true;
    let a42 : bool = a1 or false;

    let a51 : bool = a2 and true;
    let a52 : bool = a2 and false;
    let a61 : bool = a2 or true;
    let a62 : bool = a2 or false;

    let a71 : bool = not a1;
    let a72 : bool = not a2;

    let b : bool = true and true or false and false or true;
    let c : bool = a71 or a72;

    let t : i64 = if c then 1 + 2 else 3 * 4;

    let eq : bool = a1 == a31;
    let neq : bool = a1 != a32;
}
