#include "automatic.hpp"

namespace tpc {

	class BadKeywordError final: public dia::Error {
	private:
		Keyword expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected keyword `" + rift_def::keywordToStr(expected).str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadKeywordError(dia::SourcePosition pos, Keyword key): dia::Error(pos), expected(key) {}
	};

	class BadSpecialError final: public dia::Error {
	private:
		Special expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected special `" + rift_def::specialToStr(expected).str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadSpecialError(dia::SourcePosition pos, Special spec): dia::Error(pos), expected(spec) {}
	};

	class BadOperatorError final: public dia::Error {
	private:
		Operator expected;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected operator `" + rift_def::operatorToStr(expected).str() + "` here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadOperatorError(dia::SourcePosition pos, Operator opr): dia::Error(pos), expected(opr) {}
	};

	class NoIdentifierError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected an identifier here.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoIdentifierError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MaybeToken parseOne(ParserState& state, Keyword key, bool ignorable) {
		auto current = &state[0];
		if (!state.tryEat(key)) {
			state.fail(base::make_unique<BadKeywordError>(state.getPosition(), key));
			if (!ignorable) state.tokens().next();
			return {};
		}
		return {base::borrow_ptr(current)};
	}

	MaybeToken parseOne(ParserState& state, Special spec, bool ignorable) {
		auto current = &state[0];
		if (!state.tryEat(spec)) {
			state.fail(base::make_unique<BadSpecialError>(state.getPosition(), spec));
			if (!ignorable) state.tokens().next();
			return {};
		}
		return {base::borrow_ptr(current)};
	}

	MaybeToken parseOne(ParserState& state, Operator op, bool ignorable) {
		auto current = &state[0];
		if (!state.tryEat(op)) {
			state.fail(base::make_unique<BadOperatorError>(state.getPosition(), op));
			if (!ignorable) state.tokens().next();
		}
		return {base::borrow_ptr(current)};
	}

	MaybeToken parseOne(ParserState& state, Identifier* result, bool ignorable) {
		if (!state.ctokens().peek().isIdentifier()) {
			state.fail(base::make_unique<NoIdentifierError>(state.getPosition()));
			result->value = base::StrId("<error>");
			if (!ignorable) state.tokens().next();
			return {};
		}
		auto current = &state[0];
		result->value = state.tokens().next().getValue();
		return {base::borrow_ptr(current)};
	}

	MaybeToken parseOne(ParserState& state, OptionalIdentifier* result, bool) {
		if (state.ctokens().peek().isIdentifier()) {
			auto current = &state[0];
			result->value = state.tokens().next().getValue();
			return {base::borrow_ptr(current)};
		}
		return {};
	}

	void identifierDprint(base::StrId value, std::ostream& out) {
		// @TODO: change Name to Identifier
		out << "{\"Name\": ";
		if (value.isBad())
			out << "\"BAD_NAME\"";
		else
			out << "\"" << value.strView() << "\"";
		out << "}";
	}

	void nullAwareDprint(Identifier ident, std::ostream& out) {
		identifierDprint(ident.value, out);
	}

	void nullAwareDprint(OptionalIdentifier ident, std::ostream& out) {
		if (ident.value.has_value())
			identifierDprint(ident.value.value(), out);
		else
			out << "\"<ANONYMOUS>\"";
	}

}
