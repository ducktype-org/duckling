#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

#include <base/variant.hpp>

namespace pst {

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

	ParserRef<Expr> Expr::parse(RiftParserState& state, bool allow_comma) {
		// @TODO: better inf
		return Expr::parse(state, 1e18, false, allow_comma);
	}

	ParserRef<Expr>
		Expr::parseUntil(RiftParserState& state, rift_def::Operator until, bool allow_comma) {
		// look ahead:
		usize count = 0;
		while (!state.ctokens().is(until, count)) {
			if (state.ctokens().size() < count) {
				state.fail(0, "Bad expression end");
				break;
			}
			count++;
		}

		return Expr::parse(state, count, true, allow_comma);
	}

	/**
	 * It is left in this state for now, as a lot will depend on semantical analysis
	 * @TODO: lambda, todo-s
	 */
	ParserRef<Expr>
		Expr::parse(RiftParserState& state, usize len, bool exact_len, bool allow_comma) {
		auto  res = makeRef<Expr>(state.ctokens().peek().getPosition());
		auto  out = res.borrow_mut();
		usize i   = 0;

		while (state.notEmpty() and i < len) {
			i++;

			if (state.ctokens().peek().isBracketGroup()) {
				auto type = fromTokenType(state.ctokens().peek().getBracketType());
				state.goDown();
				if (state.notEmpty()) {
					out->elements.emplace_back(Group{ type, Expr::parse(state, true) });
				} else {
					out->elements.emplace_back(Group{
						type, makeRef<Expr>(state.ctokens().peek().getPosition()) });
				}
				state.goUpAndSkip();
			} else if (state.ctokens().isOperator()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(Operator({ token.getValue() }));
			} else if (state.ctokens().peek().isIdentifier()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(Identifier({ token.getValue() }));
			} else if (state.ctokens().isKeyword()) {
				// @TODO: check if keyword is legal in expr and proceed accordingly
				auto token = state.tokens().next();
				out->elements.emplace_back(KeywordValue({ token.asKeyword() }));
			} else if (state.ctokens().peek().isNumLiteral()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(NumLiteral({ token.getValue() }));
			} else if (state.ctokens().is(Special::Semicolon)) {
				break;
			} else if (state.ctokens().is(Special::Comma) && allow_comma) {
				if (res->elements.size() != 1
				    || !std::holds_alternative<CommaSeparated>(res->elements.front())) {
					auto sep = makeRef<Expr>(res->getSourcePosition());
					sep->elements.emplace_back(CommaSeparated{});
					std::get<CommaSeparated>(sep->elements.front())
						.expr.emplace_back(std::move(res));
					res = std::move(sep);
				}
				state.tokens().skip(1);
				auto new_exp = makeRef<Expr>(state.ctokens().peek().getPosition());
				out          = new_exp.borrow_mut();
				std::get<CommaSeparated>(res->elements.front())
					.expr.emplace_back(std::move(new_exp));
			}
			// @TODO: Add support for strings
			else {
				state.fail(-1, "unexpected token in expression after here");
				break;
			}
		}
		if (exact_len and i != len) {
			state.fail(-1, "expression unexpectedly ended here");
		} else if (out->elements.empty()) {
			// @IDEA: maybe we add a flag for this check, sometimes it's unnecessary
			state.fail(-1, "no expression where expression expected");
			// we have to skip because we might loop
			state.tokens().skip();
		}
		return res;
	}

	void Expr::dprint(std::ostream& out) const {
		out << "{\"Expr\" : [";
		for (auto& e: elements) {
			variant_match(e) {
				variant_case(Identifier, idt) {
					out << "{\"Identifier\": \"" << idt.indent_id.strView() << "\"}";
				}
				variant_case(Operator, oper) {
					out << "{\"Operator\": \"" << oper.oper_id.strView() << "\"}";
				}
				variant_case(NumLiteral, num) {
					out << "{\"NumLiteral\": \"" << num.num_id.strView() << "\"}";
				}
				variant_case(Group, group) {
					constexpr static std::array<std::string_view, 4> gr_strings
						= { "()", "[]", "{}", "  " };
					out << "{ \"Group\": { \"type\": \"";
					out << gr_strings[int(group.type)];
					out << "\", \"expr\": ";
					nullAwareDprint(group.expr, out);
					out << "} }";
				}
				variant_case(CommaSeparated, sep) {
					out << "{ \"Comma separated\": [";
					bool comma = false;
					for (const auto& expr: sep.expr) {
						if (comma)
							out << ", ";
						else
							comma = true;
						tpc::nullAwareDprint(expr, out);
					}
					out << "] }";
				}
				variant_case(KeywordValue, key) {
					out << "{\"KeywordValue\": \"" << rift_def::keywordToStr(key.keyword).strView()
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
