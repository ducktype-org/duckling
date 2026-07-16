#pragma once

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)                                                    \
	class_name(LangElementConstructionArgument state): ClassSpecial(StmtKind::class_name, state) { \
		this->element_kind = ElementKind::ClassSpecial;                                            \
	}
