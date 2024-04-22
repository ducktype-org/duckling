#pragma once

#include "pst_parser/elements/elements.hpp"
#include "pst_parser/rift_parser_base.hpp"
#include "pst_ref.hpp"
#include <vector>

namespace compiler::helios {
	// @TODO:
	// * future: walkers for class and stuff?
	// * ???

	using StmtList = std::vector<PstRef<pst::Stmt>>;

	StmtList getChildStmtsOf(PstRef<pst::RiftElement>);

}
