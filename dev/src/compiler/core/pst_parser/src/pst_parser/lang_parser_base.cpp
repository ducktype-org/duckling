#include "access.hpp"
#include "elements/includes/basic.hpp"
#include "lang_parser_element.hpp"
#include "lang_parser_state.hpp"

#include <base/exceptions.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <ranges>

namespace pst {
	std::string ElementPath::str() const {
		using namespace std::ranges;
		using namespace std::views;
		return elements | join_with('.') | to<std::string>();
	}

	base::Optional<AccessLocked<LangElement>> LangElement::getParent() const { return parent; }

	void LangParserState::addImport(const ImportType& import) { imports.push_back(import); }

	const dia::SourcePosition& LangElement::getSourcePosition() const { return source_position; }

	void LangElement::calcElementPathsRecursive() {
		for (auto& el: sub_elements) {
			variant_match(el) {
				variant_case(InternalChild, child) {
					CORE_PANIC(
						"Default implementation of calculating element paths cannot handle unnamed "
						"sub-elements. Encountered while calculating for: "
						+ elementType()
					);
				}
				variant_case(InternalNamedChild, named_child) {
					ElementPath child_path(getElementPath(), named_child.name);
					named_child.element->calcElementPaths(child_path);
				}
			}
		}
	}

	void LangElement::calcOrderedListChildPath(
		std::vector<AccessInternalAnonymous<Stmt>>& statements, const ElementPath& path
	) {
		auto                          no_symbol_path  = ElementPath(path, "no_symbol");
		usize                         no_symbol_count = 0;
		auto                          by_symbol_path  = ElementPath(path, "by_symbol");
		base::Map<base::StrID, usize> by_symbol_count;
		usize                         symbol_count      = 0;
		auto                          transparent_path  = ElementPath(path, "transparent");
		usize                         transparent_count = 0;

		ElementPath id_path = no_symbol_path;

		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				id_path = ElementPath(no_symbol_path, std::format("[{}]", no_symbol_count));
				calcChildPath(stmt, id_path);
				no_symbol_count++;
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getDeclSymbolName().value();
				if (by_symbol_count.atMaybe(symbol)) {
					symbol_count = by_symbol_count[symbol];
				} else {
					by_symbol_count.put(symbol);
					symbol_count = 0;
				}
				id_path
					= ElementPath(by_symbol_path, std::format("{}[{}]", symbol.str(), symbol_count));
				calcChildPath(stmt, id_path);
				by_symbol_count[symbol]++;
				break;
			case DeclKind::Transparent:
				id_path = ElementPath(transparent_path, std::format("[{}]", transparent_count));
				calcChildPath(stmt, id_path);
				transparent_count++;
				break;
			}
		}
	}

	void LangElement::calcHashRecursive() {
		calcHash();
		for(auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) {
					el->calcHashRecursive();
				}
				variant_case(InternalNamedChild, el) {
					el.element->calcHashRecursive();
				}
			}
		}
	}

	void LangElement::calcHash() {
		HashAlg partial_hash;
		addToHash(partial_hash, getElementPath());
		addToHash(partial_hash, elementType());
		hash = calcStableHash(partial_hash);

		// Can be used to turn on unstable hashing for testing purposes.
		// hash = getID().asInt();
	}

	void LangElement::addToken(CRef<tpc::Token> t) {
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void LangElement::addToken(const Box<tpc::Token>& t) { addToken(t.ref()); }

	void LangElement::addToken(const tpc::Token& t) { addToken(&t); }

	void LangElement::addChild(MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			sub_elements.emplace_back(opt.value());
			setLastToken(el->getSourcePosition());
		}
	}

	void LangElement::addNamedChild(const std::string& name, MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			sub_elements.emplace_back(InternalNamedChild{ .name = name, .element = opt.value() });
			setLastToken(el->getSourcePosition());
		}
	}

	void LangElement::setFirstToken(dia::SourcePosition pos) {
		if (pos.getStart() < source_position.getStart())
			source_position = dia::SourcePosition(pos, source_position.getEnd());
	}

	void LangElement::setLastToken(dia::SourcePosition pos) {
		if (not pos.isFileEnd() && pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

	void LangElement::acceptVisitor(PstVisitor&) const {
		CORE_PANIC("PstVisitor not supported for " + elementType());
	}

	base::HashMap<u64, AccessLocked<LangElement>> LangElement::pst_id_map{};

	AccessLocked<LangElement> LangElement::getByID(u64 id) { return pst_id_map.at(id); }
}
