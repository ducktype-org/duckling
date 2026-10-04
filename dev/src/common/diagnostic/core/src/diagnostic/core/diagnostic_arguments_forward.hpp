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
