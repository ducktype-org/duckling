#pragma once

#include "access.hpp"
#include "elements/elements_list.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/visitor.hpp>

namespace pst {
	MAKE_ACCESS_VISITOR(
		Pst,
		Import,
		StmtSpecifier,
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
		FunDecl,
		Pattern,
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
		Param,
		CallArgument,
		FlowPattern,
		AnalysisPattern,
		DeconstructorPattern,
		TuplePattern,
		WildcardPattern,
		BindingPattern,
		ValuePattern,
		Expand,
	);
}
