#include "meta.hpp"

#define CONDITION(name) static bool name(const RiftParserState& state, i64 fwd = 0)

namespace pst { 
	/**
	 * @brief Common root for expression sub-elements
	 */
	class ExprElement: public NotStmt {
		const i64 precedence;
	protected:
		static void fastForward(RiftParserState& state, i64 length) {
			state.tokens().skip(length);
		}

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

		void dprint(std::ostream& out) const override {
			out << "<SUBEXPR UNIMPLEMENTED>";
		}

		explicit ExprElement(const dia::SourcePosition& position, i64 precedence): NotStmt(position),  precedence(precedence) {}
	};

	/**
	 * @brief This should be generalized or made into separate parts in logical places.
	 */
	class ExprClassify {
	public:
		ExprClassify() = delete;

		CONDITION(isComparison) {
			static std::set<rift_def::Operator> comparisons = {
				Operator::Lesser,
				Operator::LEqual,
				Operator::Greater,
				Operator::GEqual,
				Operator::Equal,
				Operator::NotEqual,
			};
			return state[fwd].isOperator() && comparisons.contains(state[fwd].asOperator());
		}

		CONDITION(isAssignment) {
			return state[fwd].isOperator() && !isComparison(state, fwd) && state[fwd].getStrValue().back() == '=';
		}

		CONDITION(exprStmtEnd) {
			return state[fwd].is(Special::Semicolon);
		}
	};

	namespace expr {
		/**
		 * @brief General parseUntil
		 */
		template<
			std::derived_from<ExprElement> T, 
			StateCondition                  until// ,
			// StateCondition                  positiveEnd,
			// std::derived_from<dia::Message> badEndMessage
		>
		ParserRef<ExprElement> parseUntil(RiftParserState& state) {
			i64 length = 0;
			while(state.notEmpty() && !until(state, length)) {
				length++;
			}
			return T::parse(state, length);
		}

		class PrefixOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> expr;
			base::StrId type;

		public:
			explicit PrefixOperator(const dia::SourcePosition& pos, base::StrId type, i64 precedence): ExprElement(pos, precedence), type(type) {}
		};

		class SuffixOperator: public ExprElement {
		protected:
			base::StrId type;
			ParserRef<ExprElement> expr;

		public:
			explicit SuffixOperator(const dia::SourcePosition& pos, base::StrId type, i64 precedence): ExprElement(pos, precedence), type(type) {}
		};

		class BinaryOperator: public ExprElement {
		protected:
			ParserRef<ExprElement> left;
			base::StrId type;
			ParserRef<ExprElement> right;

		public:
			explicit BinaryOperator(const dia::SourcePosition& pos, base::StrId type, i64 precedence): ExprElement(pos, precedence), type(type) {}
		};

		class Value: public ExprElement {
			base::StrId number;
		public:
			explicit Value(const dia::SourcePosition& position): ExprElement(position, 0){}

			std::string elementType() const override {
				return "Value Expr";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Value" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				if (!state[0].is(lexer::Token::Type::NumLiteral)) {
					std::cerr << "Literal expected" << std::endl;
					// Literal expected error
					fastForward(state, length);
					return nullptr;
				}

				auto out = base::make_unique<Value>(pos);
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

		class LogicNot: public PrefixOperator {
			using Lower = Value;
			using Self = LogicNot;
		public:
			explicit LogicNot(const dia::SourcePosition& position): PrefixOperator(position, rift_def::keywordToStr(Keyword::Not), 730){}

			std::string elementType() const override {
				return "Logical Not";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical Not" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				if (!state[0].is(Keyword::Not)) {
					return Lower::parse(state, length);
				}

				auto out = base::make_unique<LogicNot>(pos);

				state.parse(out).one(Keyword::Not);
				state.parse(out).with(&out->expr, Self::parse, length - 1);

				return out;
			}
		};
		class LogicAnd: public ExprElement {
			using Lower = LogicNot;
			using Self = LogicAnd;

			ParserRef<ExprElement> left;
			ParserRef<ExprElement> right;
		public:
			explicit LogicAnd(const dia::SourcePosition& position): ExprElement(position, 730){}

			std::string elementType() const override {
				return "Logical And";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical And" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				bool and_found = false;
				u64 and_fwd = 0;

				for(u64 i = 0; i < length; i++) {
					if (state[i].is(Keyword::And)) {
						and_found = true;
						and_fwd = i;
						break;
					} 
				}
				if (!and_found) {
					return Lower::parse(state, length);
				}

				auto out = base::make_unique<LogicAnd>(pos);

				state.parse(out).with(&out->left, Lower::parse, +and_fwd);
				state.parse(out).one(Keyword::Or);
				state.parse(out).with(&out->right, Self::parse, length - and_fwd - 1);

				return out;
			}
		};

		class LogicOr: public ExprElement {
			using Lower = LogicAnd;
			using Self = LogicOr;

			ParserRef<ExprElement> left;
			ParserRef<ExprElement> right;
		public:
			explicit LogicOr(const dia::SourcePosition& position): ExprElement(position, 760){}

			std::string elementType() const override {
				return "Logical Or";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Logical Or" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				

				bool or_found = false;
				u64 or_fwd = 0;

				for(u64 i = 0; i < length; i++) {
					if (state[i].is(Keyword::Or)) {
						or_found = true;
						or_fwd = i;
						break;
					} 
				}
				if (!or_found) {
					return Lower::parse(state, length);
				}

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
			explicit Ternary(const dia::SourcePosition& position): ExprElement(position, 800){}

			std::string elementType() const override {
				return "Ternary Expr";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Ternary" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				bool if_found = false;
				bool then_found = false;
				u64 then_fwd = 0;
				bool else_found = false;
				u64 else_fwd = 0;

				for(u64 i = 0; i < length; i++) {
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
						if (!if_found) {
							if_found = true;
						}
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
							then_fwd = i;
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
							else_fwd = i;
						}
					}
				}
				if (!if_found) {
					return Lower::parse(state, length);
				}
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
			explicit Comma(const dia::SourcePosition& position): ExprElement(position, 900){}

			std::string elementType() const override {
				return "Comma Expr";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Comma" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				std::vector<u64> ends;
				for(u64 i = 0; i < length; i++) {
					if (state[i].is(Special::Comma)) {
						ends.push_back(i);
					}	
				}
				if (ends.empty()) {
					return Lower::parse(state, length);
				}
				auto out = base::make_unique<Comma>(pos);
				u64 start = -1;

				for(auto end: ends) {
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
		};

		class Assignment: public ExprElement {
			using Lower = Comma;

			ParserRef<ExprElement> variables;
			base::StrId type;
			ParserRef<ExprElement> value;
		public:
			explicit Assignment(const dia::SourcePosition& position): ExprElement(position, 1000){}

			std::string elementType() const override {
				return "Assignment Expr";
			}

			static ParserRef<ExprElement> parse(RiftParserState& state, u64 length) {
				std::cerr << "Parsing Assignment" << std::endl;
				if (!checkLength(state, length)) { return nullptr; }

				auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

				bool found = false;
				u64 place = 0;
				for(u64 i = 0; i < length; i++) {
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
				if (!found) {
					return Lower::parse(state, length);
				}
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
		explicit NewExprStmt(dia::SourcePosition pos): Stmt(StmtKind::ExprStmt, pos) {};

		static ParserRef<NewExprStmt> parse(RiftParserState& state) {
			auto out = base::make_unique<NewExprStmt>(state.getPosition());
			state.parse(out).with(&out->expr, expr::parseUntil<expr::Assignment, ExprClassify::exprStmtEnd>);
			return out;
		}

		std::string elementType() const override {
			return "New Expr Statement";
		}

		void dprint(std::ostream& out) const override {
			out << "<NEWEXPRSTMT UNIMPLEMENTED>";
		}

		void acceptVisitor(PstStmtVisitor&) const override {
			RIFT_PANIC("unimplemented");
		}
	};
}