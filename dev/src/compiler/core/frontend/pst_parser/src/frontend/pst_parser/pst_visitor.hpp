#pragma once

#include "access.hpp"
#include "elements/elements_list.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/visitor.hpp>

namespace pst {
	MAKE_ACCESS_VISITOR(
		Pst,
		Import,
		SpecifierBlock,
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
		IdentifierWrapper,  // @TODO: #2782 Remove this.
		If,
		While,
		Method,
		Field,
		Constructor,
		CopyConstructor,
		MoveConstructor,
		Destructor,
		ClassSpecifierBlock,
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
