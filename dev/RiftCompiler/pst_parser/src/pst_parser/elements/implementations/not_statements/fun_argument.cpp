#include "preamble.hpp"

namespace pst {
	class FunArgumentEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected end to function argument";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		FunArgumentEndError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<FunArgument> FunArgument::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<FunArgument>(position);

		state.parse(out).all(&out->name, Operator::Colon);

		state.parse(out).with<Expr>(&out->type, 
			Expr::parseUntil<
				detail::Conditions::isAssignOrComma, 
				detail::Conditions::isAssignOrComma, 
				FunArgumentEndError
			>, false
		);

		if (state.parse(out).tryEat(Operator::Assign)) {
			state.parse(out).with<Expr>(&out->initial, Expr::parse, false);
		}

		return out;
	}

	void FunArgument::dprint(std::ostream& out) const {
		out << "{\"Function Argument\": {";

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
