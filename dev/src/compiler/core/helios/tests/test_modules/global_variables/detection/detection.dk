var A: i64 = 1;

expand " var A_in_expand: i64 = 2; ";

namespace n1{
    var B: i64 = 3;  

    namespace n2{
        var C: i64 = 5;

        fun foo0(a: i32) -> i32 = {
            var i: i64 = 7;
            return a;
        }
    }
}

fun foo1(a: i32) -> i32 = {
    var h: i64 = 9;
    var i: i64 = 3;
    expand " var local_in_expand: i64 = 2; ";
    
    return a;
}
