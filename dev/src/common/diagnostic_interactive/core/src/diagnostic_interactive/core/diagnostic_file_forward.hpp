#pragma once

#include <base/pointers/box.hpp>

namespace dia_int::dia_file {

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
    struct ExploreEdge;
    struct Metadata;
    struct Message;
    struct Entity;
    struct Thread;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(dia_int::dia_file::Component);