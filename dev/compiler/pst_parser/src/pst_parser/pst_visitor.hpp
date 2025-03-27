#pragma once

#include "elements/elements_list.hpp"
#include "access.hpp"
#include <base/exceptions.hpp>
#include <base/visitor.hpp>

namespace pst {
	MAKE_ACCESS_VISITOR(
		Pst,
		Import,
		Using,
		Alias,
		ExprStmt,
		Return,
		Defer,
		Restart,
		Break,
		Redo,
		Continue,
		Throw,
		Const,
		Block,
		Namespace,
		Class,
		Fun,
		For,
		Variable,
		If,
		While,
		Method,
		Field,
		Constructor,
		CopyConstructor,
		Destructor,
		AccessBlock,
		FunParam
	);
}
