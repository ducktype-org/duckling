#include "../../hierarchy/declarations/all_declarations.hpp"
#include "../../hierarchy/lists/all_lists.hpp"                    // IWYU pragma: keep
#include "../../hierarchy/not_statements/all_not_statements.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/all_statements.hpp"
#include "preamble.hpp"

namespace pst {

	bool Stmt::trailingSemicolon() { return true; }

	namespace internal {

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
				return internal::parseStmt<If>(state);

			case Keyword::Fun:
				return internal::parseStmt<Fun>(state);

			case Keyword::FunDecl:
				return internal::parseStmt<FunDecl>(state);

			case Keyword::Pattern:
				return internal::parseStmt<Pattern>(state);

			case Keyword::While:
				return internal::parseStmt<While>(state);

			case Keyword::For:
				return internal::parseStmt<For>(state);

			case Keyword::Import:
				return internal::parseStmt<Import>(state);

			case Keyword::Using:
				return internal::parseStmt<Using>(state);

			case Keyword::Namespace:
				return internal::parseStmt<Namespace>(state);

			case Keyword::Class:
				return internal::parseStmt<Class>(state);

			case Keyword::Block:
				return internal::parseStmt<Block>(state);

			case Keyword::Const:
				return internal::parseStmt<Const>(state);

			case Keyword::Alias:
				return internal::parseStmt<Alias>(state);

			case Keyword::Var:
			case Keyword::Let:
				return internal::parseStmt<Variable>(state);

			case Keyword::Expand:
				return internal::parseStmt<Expand>(state);

			default:
				if (StmtSpecifier::SPECIFIERS.contains(as_keyword))
					return internal::parseStmt<StmtSpecifier>(state);
				break;
			}

			if (lang_def::keywordFlags(as_keyword).contains(lang_def::KeywordFlagsOptions::IsAction))
				return internal::parseStmt<Action>(state);

			if (as_special == Special::Semicolon) {
				state.tokens().skip();
				return nullptr;
			}

			// Expr as stmt have semicolon at the end:
			return internal::parseStmt<ExprStmt>(state);
		}
	}

	Stmt::AttrBoxList Stmt::collectAttributes(LangParserState& state) {
		auto        as_special = state[0].asSpecial();
		AttrBoxList attributes;

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
		MBox<Stmt> out = internal::chooseStmt(state);

		// Add Attributes
		if (out) out->addAttributes(state, std::move(attributes));

		return out;
	}

	LangElement::HashAlg& Stmt::addGenericDataToHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, attributes.size());
		return partial_hash;
	}

	void Stmt::calcElementPathsRecursive() {
		auto        path       = getElementPath();
		ElementPath attrs_path = { path, "attributes" };
		calcIndexedListChildPath<Attribute>({ attributes }, attrs_path);
		for (auto& el: sub_elements) {
			variant_match(el) {
				variant_case(InternalChild, child) {
					if (child->getElementKind() == ElementKind::Attribute) continue;
					CORE_PANIC(
						"Default implementation of calculating element paths cannot handle unnamed "
						"sub-elements. Encountered while calculating for: "
						+ elementType()
					);
				}
				variant_case(InternalNamedChild, named_child) {
					ElementPath child_path(path, named_child.name);
					named_child.element->calcElementPaths(child_path);
				}
			}
		}
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

	void Stmt::addAttributes(LangParserState& state, AttrBoxList&& additions) {
		attributes.resize(additions.size());
		usize i = 0;
		for (auto&& attr_add: std::move(additions)) {
			state.parse(Ref(this)).assign(&attributes[i], MBox(std::move(attr_add)));
			i++;
		}

		if (attributes.size() > 0)
			setFirstToken(attributes.front().internal()->getSourcePosition());
	}
}
