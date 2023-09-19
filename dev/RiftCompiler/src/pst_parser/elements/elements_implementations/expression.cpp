#include "elements_implementation.hpp"

#include <base/variant.hpp>

namespace pst {

	Expr::GroupType fromTokenType(Token::Type type) {
		switch (type) {
		case Token::Type::RoundGroup:
			return Expr::GroupType::RoundGroup;
		case Token::Type::AngleGroup:
			return Expr::GroupType::AngleGroup;
		case Token::Type::CurlyGroup:
			return Expr::GroupType::CurlyGroup;
		case Token::Type::SquareGroup:
			return Expr::GroupType::SquareGroup;
		default:
			throw std::logic_error("bad token type\n");
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
		auto  out = makeRef<Expr>(state.ctokens().peek().getPosition());
		usize i   = 0;

		while (state.notEmpty() and i < len) {
			i++;

			if (state.ctokens().peek().isGroup()) {
				auto type = fromTokenType(state.ctokens().peek().getType());
				state.goDown();
				if (state.notEmpty()) {
					out->elements.emplace_back(Group{ type, Expr::parse(state) });
				} else {
					out->elements.emplace_back(
						Group{ type, makeRef<Expr>(state.ctokens().peek().getPosition()) });
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
				out->elements.emplace_back(KeywordValue({ token.getValue() }));
			} else if (state.ctokens().peek().isNumLiteral()) {
				auto token = state.tokens().next();
				out->elements.emplace_back(NumLiteral({ token.getValue() }));
			} else if (state.ctokens().is(Special::Semicolon)) {
				break;
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
		return out;
	}

	void Expr::dprint(std::ostream& out) const {
		out << "{\"Expr\" : [";
		for (auto& e : elements) {
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
				variant_case(KeywordValue, key) {
					out << "{\"KeywordValue\": \"" << key.key_id.strView() << "\"}";
				}
				variant_default {
					RIFT_PANIC("Bad Expr alternative");
				}
			}
			out << ", ";
		}
		out << "]}";
	}
}
