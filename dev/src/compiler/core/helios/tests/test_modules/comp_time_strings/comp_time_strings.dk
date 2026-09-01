import core.builtins.*;
import core.containers.*;

# Functions returning aggregate CTVs (a `String` and a `str` char-slice). Both bodies produce valid
# Duckling source so the same functions can also drive `expand`.
fun getString() -> String = "fun fromString() -> i64 = 1;".toString();
fun getCharSlice() -> str = "fun fromSlice() -> i64 = 2;";

# Comp-time evaluation of aggregate-returning functions. `a` materializes a `String` CTV, `b` a
# char-slice CTV.
const a = getString();
const b = getCharSlice();

# `expand` injects the strings produced above as source, defining `fromString`/`fromSlice`.
namespace expanded {
	expand getString();
	expand getCharSlice();
}
