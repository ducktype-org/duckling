#include "elements_implementation.hpp"

namespace pst {
	ParserRef<ArgList> ArgList::parse(RiftParserState& state){
		// @TODO: Placeholder
		state.tokens().next();
		return ParserRef<ArgList>(new ArgList());
	}

	// @TODO: Placeholder
	void ArgList::dprint(std::ostream& out) const {
		out << "{\"ArgList\" : \"<PLACEHOLDER>\"}";
	}
}

