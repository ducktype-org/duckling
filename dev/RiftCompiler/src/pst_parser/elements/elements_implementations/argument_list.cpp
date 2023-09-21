#include "elements_implementation.hpp"

namespace pst {
	ParserRef<ArgList> ArgList::parse(RiftParserState& state){
		// @TODO: Placeholder
		auto position = state.tokens().peek().getPosition();
		state.tokens().next();
		return makeRef<ArgList>(position);
	}

	// @TODO: Placeholder
	void ArgList::dprint(std::ostream& out) const {
		out << "{\"ArgList\" : \"<PLACEHOLDER>\"}";
	}
}
