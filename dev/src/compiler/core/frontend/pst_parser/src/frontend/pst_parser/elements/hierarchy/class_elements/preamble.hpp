// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)                                                    \
	class_name(LangElementConstructionArgument state): ClassSpecial(StmtKind::class_name, state) { \
		this->element_kind = ElementKind::ClassSpecial;                                            \
	}
