
class FirstClass {
    a: i64 = 0;
    b: f32 = 0.0;
}

class ClassWithMember {
    member: FirstClass = FirstClass(3, 4.0);
    
    fun getMemberA() -> i64 = {
        return member.a;
    }
}

var c          = ClassWithMember(FirstClass(5, 6.0 as f32));
var c_member   = c.member;

var c_member_a = c.getMemberA();

