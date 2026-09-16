const x: i32 = 1235;
alias a = x;
namespace N2 {
	alias b = a;
}
alias n1 = N2;
alias n2 = N2;
using n1.*;

namespace M {
	const c = 42;
}
using M.c;

class T {
	x: i64;

	# aliases in classes not supported yet
	# using x.a.b.c.*;
}

var t: T;
# t.z; dealias to t.x.z
