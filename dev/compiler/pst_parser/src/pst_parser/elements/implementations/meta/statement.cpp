#include "../../hierarchy/declarations.hpp"
#include "../../hierarchy/statements.hpp"
#include "preamble.hpp"

namespace pst {

	bool Stmt::trailingSemicolon() { return true; }

	namespace detail {

		template<std::derived_from<Stmt> T>
		MBox<T> parseStmt(LangParserState& state) {
			MBox<T> out = T::parse(state);
			auto    opt = out.toOpt();
			if (opt && opt.value()->trailingSemicolon())
				state.parse(opt.value()).one(Special::Semicolon);
			return out;
		}

		MBox<Stmt> chooseStmt(LangParserState& state) {
			Special as_special = state[0].asSpecial();
			Keyword as_keyword = state[0].asKeyword();

			switch (as_keyword) {
			case Keyword::If:
				return detail::parseStmt<If>(state);

			case Keyword::Fun:
				return detail::parseStmt<Fun>(state);

			case Keyword::While:
				return detail::parseStmt<While>(state);

			case Keyword::For:
				return detail::parseStmt<For>(state);

			case Keyword::Import:
				return detail::parseStmt<Import>(state);

			case Keyword::Using:
				return detail::parseStmt<Using>(state);

			case Keyword::Namespace:
				return detail::parseStmt<Namespace>(state);

			case Keyword::Class:
				return detail::parseStmt<Class>(state);

			case Keyword::Block:
				return detail::parseStmt<Block>(state);

			case Keyword::Const:
				return detail::parseStmt<Const>(state);

			case Keyword::Alias:
				return detail::parseStmt<Alias>(state);

			case Keyword::Var:
			case Keyword::Let:
				return detail::parseStmt<Variable>(state);

			default:
				break;
			}

			if (lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsAction))
				return detail::parseStmt<Action>(state);

			if (as_special == Special::Semicolon) {
				state.tokens().skip();
				return nullptr;
			}

			// Expr as stmt have semicolon at the end:
			return detail::parseStmt<ExprStmt>(state);
		}
	}

	Stmt::AttrList Stmt::collectAttributes(LangParserState& state) {
		auto     as_special = state[0].asSpecial();
		AttrList attributes;

		while (as_special == Special::AtSign) {
			MBox<Attribute> attr = Attribute::parse(state);
			auto            opt  = std::move(attr).toOptBox();
			if (opt) attributes.emplace_back(std::move(opt.value()));
			as_special = state[0].asSpecial();
		}
		return attributes;
	}

	MBox<Stmt> Stmt::parse(LangParserState& state) {
		// Collect Attributes
		auto attributes = collectAttributes(state);

		// Parse Statement
		MBox<Stmt> out = detail::chooseStmt(state);

		// Add Attributes
		if (out) out->addAttributes(std::move(attributes));

		return out;
	}

	void Stmt::dprintPrefix(std::ostream& out) const {
		LangElement::dprintPrefix(out);
		dprintAttributes(out);
	}

	void Stmt::dprintAttributes(std::ostream& out) const {
		if (not attributes.empty()) {
			out << R"("attributes": [)";
			for (auto& attribute: attributes) {
				attribute.internal()->debugPrint(out);
				out << ",";
			}
			out << "],";
		}
	}

	void Stmt::addAttributes(AttrList&& additions) {
		attributes = std::move(additions);

		using namespace std::views;
		auto borrow = [](AccessInternal<Attribute>& arg) -> Child { return arg.give(); };
		auto borrowed_additions = attributes | transform(borrow);

		sub_elements.insert(
			sub_elements.end(), borrowed_additions.begin(), borrowed_additions.end()
		);

		if (attributes.size() > 0)
			setFirstToken(attributes.front().internal()->getSourcePosition());
	}
}
