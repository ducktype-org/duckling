#pragma once

#include <base/pointers/box.hpp>

namespace tpc {
	class TokenStream;
}

namespace pst {
	class LangParserState;
	using tpc::TokenStream;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::LangParserState)
