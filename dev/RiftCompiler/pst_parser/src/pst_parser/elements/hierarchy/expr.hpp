#pragma once

#include "meta.hpp"
#include "lists.hpp"           // IWYU pragma: keep
#include "not_statements.hpp"  // IWYU pragma: keep

#include <stack>

#define CONDITION(name) static bool name(const RiftParserState& state, i64 fwd = 0)

namespace pst {
	class CodeBlock;

	/**
	 * @brief Common root for expression sub-elements
	 */
	class ExprElement: public NotStmt {
		const i64 precedence;

	protected:
		static void fastForward(RiftParserState& state, i64 length) { state.tokens().skip(length); }

		static bool checkLength(RiftParserState& state, i64 length) {
			if (length == 0) {
				std::cerr << "empty expression" << std::endl;
				// Empty expression error
				fastForward(state, length);
				return false;
			}
			if (state[length - 1].is(lexer::Token::Type::Sentinel)) {
				std::cerr << "too long expression" << std::endl;
				// Expression length too long error
				fastForward(state, length);
				return false;
			}
			return true;
		}

		void dprint(std::ostream& out) const override { out << "<SUBEXPR UNIMPLEMENTED>"; }

		explicit ExprElement(const dia::SourcePosition& position, i64 precedence):
			  NotStmt(position),
			  precedence(precedence) {}
	};

	/**
	 * @brief This should be generalized or made into separate parts in logical places.
	 */
	class ExprClassify {
	public:
		ExprClassify() = delete;

		CONDITION(isComparison) {
			static std::set<rift_def::Operator> comparisons = {
				Operator::Lesser, Operator::LEqual, Operator::Greater,
				Operator::GEqual, Operator::Equal,  Operator::NotEqual,
			};
			return state[fwd].isOperator() && comparisons.contains(state[fwd].asOperator());
		}

		CONDITION(isAssignment) {
			return state[fwd].isOperator() && !isComparison(state, fwd)
			    && state[fwd].getStrValue().back() == '=';
		}

		CONDITION(exprStmtEnd) { return state[fwd].is(Special::Semicolon); }

		CONDITION(isOperator) { return state[fwd].isOperator(); }
	};

	namespace expr {
		/**
		 * @brief General parseUntil
		 */
		template<
			std::derived_from<ExprElement> T,
			StateCondition                 until  // ,
		                                          // StateCondition                  positiveEnd,
		                                          // std::derived_from<dia::Message> badEndMessage
			>
		ParserRef<ExprElement> parseUntil(RiftParserState& state) {
			i64 length = 0;
			while (!state[length].is(lexer::Token::Type::Sentinel) && !until(state, length))
				length++;
			return T::parse(state, length);
		}

		class PrefixOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> expr;
			base::StrId            type;

		public:
			explicit PrefixOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Prefix Operator"; }
		};

		class SuffixOperator: public ExprElement {
		protected:
			base::StrId            type;
			ParserRef<ExprElement> expr;

		public:
			explicit SuffixOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Suffix Operator"; }
		};

		class BinaryOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> left;
			base::StrId            type;
			ParserRef<ExprElement> right;

		public:
			explicit BinaryOperator(
				const dia::SourcePosition& pos, base::StrId type, i64 precedence
			):
				  ExprElement(pos, precedence),
				  type(type) {}

			std::string elementType() const override { return type.str() + " Infix Operator"; }
		};

		class Value: public ExprElement {
			base::StrId number;

		public:
			explicit Value(const dia::SourcePosition& position): ExprElement(position, 0) {}

			std::string elementType() const override { return "Value Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Value" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

				if (!state[0].is(lexer::Token::Type::NumLiteral)) {
					std::cerr << "Literal expected" << std::endl;
					// Literal expected error
					fastForward(state, length);
					return nullptr;
				}

				auto out    = base::make_unique<Value>(pos);
				out->number = state[0].getValue();
				state.parse(out).eatOne();

				if (length > 1) {
					std::cerr << "Bad value length" << std::endl;
					// Bad value length error
					fastForward(state, length - 1);
				}

				return out;
			}
		};

		class TemplateSpecifier final: public ExprElement {
			ParserRef<ExprElement> inner;

		public:
			TemplateSpecifier(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Template Specifier Expression"; }
		};

		class IdentifierLiteral: public ExprElement {
			tpc::Identifier                        name;
			base::Optional<ParserRef<ExprElement>> template_specifier;

		public:
			IdentifierLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Literal Expression"; }
		};

		class Access: public ExprElement {
			base::StrId                            type;  ///< either `.` or `.?`
			tpc::Identifier                        name;
			base::Optional<ParserRef<ExprElement>> template_specifier;

		public:
			Access(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Access Expression"; }
		};

		class Call: public ExprElement {
			lexer::Token::BracketType type;  ///< either Round or Square
			ParserRef<ExprElement>    args;

		public:
			Call(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Call Expression"; }
		};

		/**
		 * @brief Combined Access / Call / Subscirpt.
		 */
		class ChainExpr: public ExprElement {
			using Lower = IdentifierLiteral;

			ParserRef<ExprElement>              literal;
			std::vector<ParserRef<ExprElement>> chain;

			/**
			 * @brief checks length before the start of the next link
			 */
			static u64 toNextLink(const RiftParserState& state, u64 length);

		public:
			ChainExpr(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Chain Expression"; }
		};

		class RoundExpr: public ExprElement {
			ParserRef<ExprElement> expr;

		public:
			explicit RoundExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Round Group Expression"; }
		};

		class BlockExpr: public ExprElement {
			ParserRef<CodeBlock> block;

		public:
			explicit BlockExpr(const dia::SourcePosition& pos): ExprElement(pos, 200) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length);

			std::string elementType() const override { return "Block Expression"; }
		};

		class GeneralPrefix: public PrefixOperator {
			using Lower = ChainExpr;
			using Self  = GeneralPrefix;

		public:
			explicit GeneralPrefix(const dia::SourcePosition& pos, base::StrId op):
				  PrefixOperator(pos, op, 400) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing General Prefix Expressions" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				if (!state[0].isOperator()) return Lower::parse(state, length);

				auto out
					= base::make_unique<GeneralPrefix>(state.getPosition(), state[0].getValue());

				state.parse(out).eatOne();
				state.parse(out).with(&out->expr, parse, length - 1);

				return out;
			}
		};

		class GeneralSuffix: public SuffixOperator {
			using Lower = GeneralPrefix;
			using Self  = GeneralSuffix;

			static ParserRef<ExprElement>
				parseRecursive(RiftParserState& state, u64 length, u64 iter) {
				if (iter == 0) return Lower::parse(state, length);

				auto out = base::make_unique<GeneralSuffix>(
					state.getPosition(), state[length - 1].getValue()
				);

				state.parse(out).with(&out->expr, parseRecursive, length - 1, iter - 1);

				state.parse(out).eatOne();

				return out;
			}

		public:
			explicit GeneralSuffix(const dia::SourcePosition& pos, base::StrId op):
				  SuffixOperator(pos, op, 450) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing General Suffix Expressions" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				u64 fwd            = 0;
				u64 reduced_length = length;
				// Here this should include the prefix word operators in the future
				while (fwd < length && state[fwd].isOperator()) fwd++;
				while (fwd < reduced_length && state[reduced_length - 1].isOperator())
					reduced_length--;
				if (fwd == reduced_length) {}  // Error

				if (fwd + 1 < reduced_length && state[reduced_length - 1].isIdentifier()
				    && !state[reduced_length - 2].is(Operator::Period))
					reduced_length--;
				return parseRecursive(state, length, length - reduced_length);
			}
		};

		class GeneralBinary: public BinaryOperator {
			using Lower = GeneralSuffix;
			using Self  = GeneralBinary;

			/**
			 * @brief is a general binary operators
			 */
			CONDITION(isGenBinOp) {
				static const std::set<rift_def::Operator> gen_bin_ops = {
					Operator::Multiply,
					Operator::Divide,
					Operator::Plus,
					Operator::Minus,
				};
				return gen_bin_ops.contains(state[fwd].asOperator());
			}

			static i64 getOpPrec(Operator op) {
				static const std::unordered_map<rift_def::Operator, i64> precedences = {
					{ Operator::Multiply, 510 },
					{ Operator::Divide, 510 },
					{ Operator::Plus, 520 },
					{ Operator::Minus, 520 },
				};
				return precedences.at(op);
			}

			struct OperatorBuilder;

			using BuilderExpr = std::variant<u64, base::unique_ptr<OperatorBuilder>>;

			struct OperatorBuilder {
				BuilderExpr lhs;
				Operator    type;
				BuilderExpr rhs;
			};

		public:
			explicit GeneralBinary(const dia::SourcePosition& pos, rift_def::Operator op):
				  BinaryOperator(pos, rift_def::operatorToStr(op), getOpPrec(op)) {}

			/**
			 * @brief
			 *
			 * @note Assumes an expression "literal" ends on either:
			 * 1. End of expression
			 * 2. A General binary operator
			 * 3. Literal that isn't following an access operator (`.`, in future also `.?`, maybe
			 * `::`)
			 *
			 */
			static u64 skipLiteral(const RiftParserState& state, u64 base, u64 length) {
				u64 fwd = base;
				if (state[fwd].isIdentifier()) { fwd++; }  // Ignores first identifier
				while (fwd < length && !isGenBinOp(state, fwd)
				       && !(state[fwd].isIdentifier() && !state[fwd - 1].is(Operator::Period))) {
					fwd++;
				}
				return fwd;
			}

			static ParserRef<ExprElement>
				parseRecursive(RiftParserState& state, const BuilderExpr& expr) {
				if (std::holds_alternative<u64>(expr)) {
					return Lower::parse(state, std::get<u64>(expr));
				} else {
					auto op  = std::get<base::unique_ptr<OperatorBuilder>>(expr).borrow();
					auto out = base::make_unique<GeneralBinary>(state.getPosition(), op->type);

					state.parse(out).with(&out->left, parseRecursive, op->lhs);
					state.parse(out).one(op->type);
					state.parse(out).with(&out->right, parseRecursive, op->rhs);

					return out;
				}
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing General Binary Expressions" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				u64 fwd            = 0;
				u64 reduced_length = length;
				// Here this should include the prefix word operators in the future
				while (fwd < length && state[fwd].isOperator()) fwd++;
				while (fwd < reduced_length && state[reduced_length - 1].isOperator())
					reduced_length--;
				if (fwd == reduced_length) {}  // Error

				std::vector<u64> operators;
				u64              next;
				while (fwd < reduced_length) {
					next = skipLiteral(state, fwd, reduced_length);
					if (fwd == next) {}             // Error
					if (next < reduced_length - 1)  // Not a suffix operator or end of expression
						operators.push_back(next);
					fwd = std::min(next + 1, reduced_length);
				}

				if (operators.size() == 0) return Lower::parse(state, length);

				struct Partial {
					BuilderExpr lhs;
					u64         op_place;
					i64         op_prec;
				};

				std::stack<Partial> stack;
				fwd = operators[0];
				stack.push({ fwd, fwd, getOpPrec(state[fwd].asOperator()) });

				for (u64 i = 1; i < operators.size(); i++) {
					fwd                   = operators[i];
					i64         curr_prec = getOpPrec(state[fwd].asOperator());
					BuilderExpr lhs       = operators[i] - operators[i - 1] - 1;
					while (!stack.empty() && stack.top().op_prec <= curr_prec) {
						Partial partial = std::move(stack.top());
						stack.pop();
						lhs = base::make_unique<OperatorBuilder>(
							std::move(partial.lhs),
							state[partial.op_place].asOperator(),
							std::move(lhs)
						);
					}
					stack.push({ std::move(lhs), fwd, curr_prec });
				}

				BuilderExpr rhs = length - operators.back() - 1;
				while (!stack.empty()) {
					Partial partial = std::move(stack.top());
					stack.pop();
					rhs = base::make_unique<OperatorBuilder>(
						std::move(partial.lhs), state[partial.op_place].asOperator(), std::move(rhs)
					);
				}

				return GeneralBinary::parseRecursive(state, rhs);
			}
		};

		class LogicNot: public PrefixOperator {
			using Lower = GeneralBinary;
			using Self  = LogicNot;

		public:
			explicit LogicNot(const dia::SourcePosition& position):
				  PrefixOperator(position, rift_def::keywordToStr(Keyword::Not), 730) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical Not" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

				if (!state[0].is(Keyword::Not)) return Lower::parse(state, length);

				auto out = base::make_unique<LogicNot>(pos);

				state.parse(out).one(Keyword::Not);
				state.parse(out).with(&out->expr, Self::parse, length - 1);

				return out;
			}
		};

		class LogicAnd: public BinaryOperator {
			using Lower = LogicNot;
			using Self  = LogicAnd;

		public:
			explicit LogicAnd(const dia::SourcePosition& position):
				  BinaryOperator(position, rift_def::keywordToStr(rift_def::Keyword::And), 730) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical And" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

				bool and_found = false;
				u64  and_fwd   = 0;

				for (u64 i = 0; i < length; i++) {
					if (state[i].is(Keyword::And)) {
						and_found = true;
						and_fwd   = i;
						break;
					}
				}
				if (!and_found) return Lower::parse(state, length);

				auto out = base::make_unique<LogicAnd>(pos);

				state.parse(out).with(&out->left, Lower::parse, +and_fwd);
				state.parse(out).one(Keyword::Or);
				state.parse(out).with(&out->right, Self::parse, length - and_fwd - 1);

				return out;
			}
		};

		class LogicOr: public BinaryOperator {
			using Lower = LogicAnd;
			using Self  = LogicOr;

		public:
			explicit LogicOr(const dia::SourcePosition& position):
				  BinaryOperator(position, rift_def::keywordToStr(rift_def::Keyword::Or), 760) {}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical Or" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);


				bool or_found = false;
				u64  or_fwd   = 0;

				for (u64 i = 0; i < length; i++) {
					if (state[i].is(Keyword::Or)) {
						or_found = true;
						or_fwd   = i;
						break;
					}
				}
				if (!or_found) return Lower::parse(state, length);

				auto out = base::make_unique<LogicOr>(pos);

				state.parse(out).with(&out->left, Lower::parse, +or_fwd);
				state.parse(out).one(Keyword::Or);
				state.parse(out).with(&out->right, Self::parse, length - or_fwd - 1);

				return out;
			}
		};

		class Ternary: public ExprElement {
			using Lower = LogicOr;

			ParserRef<ExprElement> condition;
			ParserRef<ExprElement> if_true;
			ParserRef<ExprElement> if_false;

		public:
			explicit Ternary(const dia::SourcePosition& position): ExprElement(position, 800) {}

			std::string elementType() const override { return "Ternary Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Ternary" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

				bool if_found   = false;
				bool then_found = false;
				u64  then_fwd   = 0;
				bool else_found = false;
				u64  else_fwd   = 0;

				for (u64 i = 0; i < length; i++) {
					if (state[i].is(Keyword::If)) {
						if (if_found) {
							std::cerr << "Ternary error 1" << std::endl;
							// Partial ternary expression error
							fastForward(state, length);
							return nullptr;
						}
						if (i != 0) {
							std::cerr << "Ternary error 2" << std::endl;
							// ternary expression in improper context error
							fastForward(state, length);
							return nullptr;
						}
						if (!if_found) if_found = true;
					} else if (state[i].is(Keyword::Then)) {
						if (!if_found) {
							std::cerr << "Ternary error 3" << std::endl;
							// Partial ternary expression error
							fastForward(state, length);
							return nullptr;
						}
						if (then_found) {
							std::cerr << "Ternary error 4" << std::endl;
							// multiple ternary in one expression error
							fastForward(state, length);
							return nullptr;
						}
						if (!then_found) {
							then_found = true;
							then_fwd   = i;
						}
					} else if (state[i].is(Keyword::Else)) {
						if (!if_found || !then_found) {
							std::cerr << "Ternary error 5" << std::endl;
							// Partial ternary expression error
							fastForward(state, length);
							return nullptr;
						}
						if (else_found) {
							std::cerr << "Ternary error 6" << std::endl;
							// multiple ternary in one expression error
							fastForward(state, length);
							return nullptr;
						}
						if (!else_found) {
							else_found = true;
							else_fwd   = i;
						}
					}
				}
				if (!if_found) return Lower::parse(state, length);
				if (if_found && !else_found) {
					std::cerr << "Ternary error 7" << std::endl;
					// Partial ternary expression error
					fastForward(state, length);
					return nullptr;
				}

				auto out = base::make_unique<Ternary>(pos);

				state.parse(out).one(Keyword::If);
				state.parse(out).with(&out->condition, Lower::parse, then_fwd - 1);

				state.parse(out).one(Keyword::Then);
				state.parse(out).with(&out->if_true, Lower::parse, else_fwd - then_fwd - 1);

				state.parse(out).one(Keyword::Else);
				state.parse(out).with(&out->if_false, Lower::parse, length - else_fwd - 1);
				return out;
			}
		};

		class Comma: public ExprElement {
			using Lower = Ternary;

			std::vector<ParserRef<ExprElement>> expressions;

		public:
			explicit Comma(const dia::SourcePosition& position): ExprElement(position, 900) {}

			std::string elementType() const override { return "Comma Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Comma" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

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
					state.parse(out).with(
						&out->expressions.back(), Lower::parse, length - 1 - start
					);
				}
				return out;
			}
		};

		class Assignment: public ExprElement {
			using Lower = Comma;

			ParserRef<ExprElement> variables;
			base::StrId            type;
			ParserRef<ExprElement> value;

		public:
			explicit Assignment(const dia::SourcePosition& position):
				  ExprElement(position, 1'000) {}

			std::string elementType() const override { return "Assignment Expr"; }

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Assignment" << std::endl;
				if (!checkLength(state, length)) return nullptr;

				auto pos = dia::SourcePosition(
					state.getPosition(), state.getPosition(length - 1).getEnd()
				);

				bool found = false;
				u64  place = 0;
				for (u64 i = 0; i < length; i++) {
					if (ExprClassify::isAssignment(state, i)) {
						if (!found) {
							found = true;
							place = i;
						} else {
							std::cerr << "multiple assignments" << std::endl;
							// Multiple assignments in one expression
							fastForward(state, length);
							return nullptr;
						}
					}
				}
				if (!found) return Lower::parse(state, length);
				auto out = base::make_unique<Assignment>(pos);

				state.parse(out).with(&out->variables, Lower::parse, +place);

				out->type = state[0].getValue();
				state.parse(out).eatOne();

				state.parse(out).with(&out->value, Lower::parse, length - place - 1);

				return out;
			}
		};
	}

	class NewExprStmt: public Stmt {
		ParserRef<ExprElement> expr;

	public:
		explicit NewExprStmt(dia::SourcePosition pos): Stmt(StmtKind::ExprStmt, pos){};

		static ParserRef<NewExprStmt> parse(RiftParserState& state) {
			auto out = base::make_unique<NewExprStmt>(state.getPosition());
			state.parse(out)
				.with(&out->expr, expr::parseUntil<expr::Assignment, ExprClassify::exprStmtEnd>);
			return out;
		}

		std::string elementType() const override { return "New Expr Statement"; }

		void dprint(std::ostream& out) const override { out << "<NEWEXPRSTMT UNIMPLEMENTED>"; }

		void acceptVisitor(PstStmtVisitor&) const override { RIFT_PANIC("unimplemented"); }
	};
}
