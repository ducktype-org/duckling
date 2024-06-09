#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

#include <base/variant.hpp>

namespace pst {
	class BadTokenError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected token in expression.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadTokenError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class UnexpectedExprEndError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected end to an expression.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		UnexpectedExprEndError(dia::SourcePosition pos, dia::SourcePosition expected):
			  dia::Error(pos) {
			addNote(base::make_unique<ExpectedEnd>(expected));
		}

		class ExpectedEnd final: public dia::NoteWithPosition {
		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				return "Expected it to end here.";
			}

		public:
			ExpectedEnd(dia::SourcePosition pos): dia::NoteWithPosition(pos) {}
		};
	};

	Expr::GroupType fromTokenType(Token::BracketType type) {
		switch (type) {
		case Token::BracketType::Round:
			return Expr::GroupType::RoundGroup;
		case Token::BracketType::Angle:
			return Expr::GroupType::AngleGroup;
		case Token::BracketType::Curly:
			return Expr::GroupType::CurlyGroup;
		case Token::BracketType::Square:
			return Expr::GroupType::SquareGroup;
		default:
			throw std::logic_error("unsupported bracket type\n");
		}
	}

	ParserRef<Expr> Expr::parse(RiftParserState& state) {
		// @TODO: better inf
		return Expr::parse(state, 1e18, false);
	}

	/**
	 * It is left in this state for now, as a lot will depend on semantical analysis
	 * @TODO: lambda, todo-s
	 */
	ParserRef<Expr> Expr::parse(RiftParserState& state, usize len, bool exact_len) {
		auto  out          = makeRef<Expr>(state.getPosition());
		usize i            = 0;
		auto  expected_end = state.getPosition((i64) len);

		RIFT_ASSERT(
			len > 0, state.getPosition().genStr("Expr parse should have positive expected length.")
		);

		if (state.empty()) {
			state.fail(base::make_unique<EmptyExprError>(state.getPosition()));
			return nullptr;
		}
		if (state[0].is(Special::Semicolon)) {
			auto err_pos = state.getPosition(-1, 0);
			state.fail(base::make_unique<EmptyExprError>(err_pos));
			return nullptr;
		}

		while (state.notEmpty() and i < len) {
			i++;

			if (state[0].isBracketGroup()) {
				auto type = fromTokenType(state[0].getBracketType());
				state.goDown();
				if (state.notEmpty())
					out->elements.emplace_back(Group{ type, Expr::parse(state) });
				else
					out->elements.emplace_back(Group{ type, makeRef<Expr>(state.getPosition()) });
				state.goUpAndSkip();
			} else if (state[0].isOperator()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(Operator({ token.getValue() }));
			} else if (state[0].isIdentifier()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(Identifier({ token.getValue() }));
			} else if (state[0].isKeyword()) {
				// @TODO: check if keyword is legal in expr and proceed accordingly
				auto token = state.tokens().next();
				out->elements.emplace_back(KeywordValue({ token.asKeyword() }));
			} else if (state[0].isNumLiteral()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(NumLiteral({ token.getValue() }));
			} else if (state[0].is(Special::Semicolon)) {
				break;
			}
			// @TODO: Add support for strings
			else {
				state.fail(base::make_unique<BadTokenError>(state.getPosition()));
				if (i == 1) return nullptr;
				break;
			}
		}
		if (exact_len and i != len) {
			auto bad_end = state.getPosition(-1);
			state.fail(base::make_unique<UnexpectedExprEndError>(bad_end, expected_end));
		}
		return out;
	}

	void Expr::dprint(std::ostream& out) const {
		out << "{\"Expr\" : [";
		for (auto& e: elements) {
			variant_match(e) {
				variant_case(Identifier, idt) {
					out << R"({"Identifier": ")" << idt.indent_id.strView() << "\"}";
				}
				variant_case(Operator, oper) {
					out << R"({"Operator": ")" << oper.oper_id.strView() << "\"}";
				}
				variant_case(NumLiteral, num) {
					out << R"({"NumLiteral": ")" << num.num_id.strView() << "\"}";
				}
				variant_case(Group, group) {
					constexpr static std::array<std::string_view, 4> gr_strings
						= { "()", "[]", "{}", "  " };
					out << R"({ "Group": { "type": ")";
					out << gr_strings.at(int(group.type));
					out << R"(", "expr": )";
					nullAwareDprint(group.expr, out);
					out << "} }";
				}
				variant_case(KeywordValue, key) {
					out << R"({"KeywordValue": ")" << rift_def::keywordToStr(key.keyword).strView()
						<< "\"}";
				}
				variant_default { RIFT_PANIC("Bad Expr alternative"); }
			}
			out << ", ";
		}
		out << "]}";
	}

	void Expr::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitExpr(*this); }
}
