#include "elements_implementation.hpp"

namespace pst {
	class BlockStartError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a code block starting with `{`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BlockStartError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	ParserRef<CodeBlock> CodeBlock::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<CodeBlock>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(base::make_unique<BlockStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();

		// @TODO: this may not work in case of compilation error
		while (state.notEmpty()) {
			ParserRef<Stmt> stmt;
			state.parse(out).one(&stmt);
			out->statements.emplace_back(std::move(stmt));
		}

		state.parse(out).goUpAndSkip();
		return out;
	}

	void CodeBlock::dprint(std::ostream& out) const {
		out << "{\"CodeBlock\": [";
		for (auto& stmt: statements) {
			nullAwareDprint(stmt, out);
			out << ", ";
		}
		out << "]}";
	}
}
