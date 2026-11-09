// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/box.hpp>

namespace dia::dia_args {

	struct Component;

	struct TextComponent;
	struct CodeComponent;
	struct CodeLocationComponent;
	struct StartLineComponent;
	struct ConcatComponent;
	struct PointedComponent;
	struct VariantComponent;
	struct LinkComponent;
	struct EvaluatedTemplateComponent;
	struct MessageIDComponent;

	struct PointerMessage;
	struct ExploreLink;
	struct Metadata;
	struct Message;
	struct Entity;
	struct Diagnostic;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia::dia_args::Component);
DEFAULT_BOX_PTR_DELETER_DECLARATION(dia::dia_args::Diagnostic);
