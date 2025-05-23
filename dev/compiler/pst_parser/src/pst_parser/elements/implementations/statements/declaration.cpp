#include "../../hierarchy/statements/declaration.hpp"

#include "../../hierarchy/lists.hpp"           // IWYU pragma: keep
#include "../../hierarchy/not_statements.hpp"  // IWYU pragma: keep

namespace pst {
	bool Decl::trailingSemicolon() { return false; }
}
