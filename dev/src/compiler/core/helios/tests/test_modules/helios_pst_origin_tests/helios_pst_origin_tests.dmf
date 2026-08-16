namespace NS1 {
    class B {
        x: i64
    }

    class A{
        b: B;
    }
    var a: A = A(B(7));
    var box_a: box A = new A(B(7));

    var smaller_int: i32 = 5;

    namespace NS2 {
        const A: i64 = 7;
    }
}

var var1: i64 = (NS1).NS2.A;
var var2: i64 = ((NS1).NS2).A;
var var3: u32 = (((NS1).NS2).A) as u32;
var var4: i64 = ((NS1).NS2).A + 2;
var var5: i64 = NS1.a.b.x;
var var6: i64 = NS1.box_a.b.x;  

var var7_generated: i64 = NS1.smaller_int + 2; # coercion and i64 cast will be here
var var8_generated: i64 = &NS1.a.b.x; # deref expression will be here

fun a() -> i64 = {
    return (NS1).NS2.A;
}

fun b() = (NS1).NS2.A;


class C {
    x: i64;
}
