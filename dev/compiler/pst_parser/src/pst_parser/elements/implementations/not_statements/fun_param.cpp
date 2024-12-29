#include "preamble.hpp"

namespace pst {
	class FunParamEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected end to function parameter";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		FunParamEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<FunParam> FunParam::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<FunParam>(position);

		state.parse(out).all(&out->name, NamedOperator::Colon);

		state.parse(out).one(&out->type);

		if (state.parse(out).tryEat(NamedOperator::Assign))
			state.parse(out).one(&out->initial);

		return out;
	}

	void FunParam::dprint(std::ostream& out) const {
		out << "{\"Function Parameter\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(,"type": )";
		nullAwareDprint(type, out);
		if (initial) {
			out << R"(,"initial": )";
			nullAwareDprint(initial.value(), out);
		}

		out << "}}";
	}
}
