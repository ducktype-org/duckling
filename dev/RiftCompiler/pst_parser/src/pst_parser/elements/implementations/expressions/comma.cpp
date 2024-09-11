#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Comma::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Comma" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		std::vector<u64> ends;
		for (u64 i = 0; i < length; i++)
			if (state[i].is(Special::Comma)) ends.push_back(i);
		if (ends.empty()) return Lower::parse(state, length);
		auto out   = base::make_unique<Comma>(pos);
		u64  start = -1;

		for (auto end: ends) {
			out->expressions.emplace_back();
			state.parse(out).with(&out->expressions.back(), Lower::parse, end - 1 - start);
			state.parse(out).one(Special::Comma);
			start = end;
		}
		if (start + 1 != length) {
			out->expressions.emplace_back();
			state.parse(out).with(&out->expressions.back(), Lower::parse, length - 1 - start);
		}
		return out;
	}
}
