#include "lang_parser_element.hpp"

#include "access.hpp"
#include "elements/includes/basic.hpp"
#include "lang_parser_state.hpp"
#include "stable_position.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace pst {

	base::Optional<AccessLocked<LangElement>> LangElement::getParent() const { return parent; }

	const dia::SourcePosition& LangElement::getSourcePosition() const { return source_position; }

	void LangElement::calcElementPathHashRecursive() {
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
					hashing::ComponentHash child_path(getElementPathHash(), named_child.name);
					named_child.element->calcElementPathHash(child_path);
				}
			}
		}
	}

	void LangElement::calcOrderedListChildPath(
		std::vector<AccessInternalAnonymous<Stmt>>& statements, const hashing::ComponentHash& path
	) {
		auto                          no_symbol_path  = hashing::ComponentHash(path, "no_symbol");
		usize                         no_symbol_count = 0;
		auto                          by_symbol_path  = hashing::ComponentHash(path, "by_symbol");
		base::Map<base::StrID, usize> by_symbol_count;
		usize                         symbol_count = 0;
		auto  transparent_path                     = hashing::ComponentHash(path, "transparent");
		usize transparent_count                    = 0;

		hashing::ComponentHash id_path = no_symbol_path;

		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				id_path
					= hashing::ComponentHash(no_symbol_path, std::format("[{}]", no_symbol_count));
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
				id_path = hashing::ComponentHash(
					by_symbol_path, std::format("{}[{}]", symbol.str(), symbol_count)
				);
				calcChildPath(stmt, id_path);
				by_symbol_count[symbol]++;
				break;
			case DeclKind::Transparent:
				id_path = hashing::ComponentHash(
					transparent_path, std::format("[{}]", transparent_count)
				);
				calcChildPath(stmt, id_path);
				transparent_count++;
				break;
			}
		}
	}

	void LangElement::calcHashRecursive() {
		calcHash();
		for (auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) { el->calcHashRecursive(); }
				variant_case(InternalNamedChild, el) { el.element->calcHashRecursive(); }
			}
		}
	}

	void LangElement::calcHash() {
		auto partial_hash = calcStableHash();
		addToHash(partial_hash, context_hash);
		hash = partial_hash.finalize();

		pst_hash_map.putOrAssign(hash.value(), AccessLocked<LangElement>(CRef<LangElement>(this)));
		
		// Can be used to turn on unstable hashing for testing purposes.
		// hash = getID().asInt();
	}

	HashAlg LangElement::calcStableHash() const {
		HashAlg partial_hash = getElementPathHash().partial;
		addToHash(partial_hash, elementType());
		addGenericDataToHash(partial_hash);
		addElementDataToStableHash(partial_hash);
		return partial_hash;
	}

	HashAlg& LangElement::addGenericDataToHash(HashAlg& partial_hash) const { return partial_hash; }

	void LangElement::calcSignature(HashAlg& partial_hash) const {
		addToHash(partial_hash, hash->data);
		for (auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) { addToHash(partial_hash, el->getHash().data); }
				variant_case(InternalNamedChild, el) {
					addToHash(partial_hash, el.element->getHash().data);
				}
				variant_case(SubToken, el) {}
				variant_default { CORE_PANIC("Unhandled variant case"); }
			}
		}
		addToHash(partial_hash, "hash_end");
	}

	void LangElement::signGenerated(HashType& signature) {
		HashAlg new_hash;
		addToHash(new_hash, hash->data);
		addToHash(new_hash, signature.data);
		hash = new_hash.finalize();
		for (auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) { el->signGenerated(signature); }
				variant_case(InternalNamedChild, el) { el.element->signGenerated(signature); }
				variant_case(SubToken, el) {}
				variant_default { CORE_PANIC("Unhandled variant case"); }
			}
		}
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

	concurrent::ConHashMap<query::QueryStableHash, AccessLocked<LangElement>>
		LangElement::pst_hash_map{};

	AccessLocked<LangElement> LangElement::getByStableHash(query::QueryStableHash stable_hash) {
		CORE_ASSERT(pst_hash_map.contains(stable_hash), "Invalid stable hash");
		return *pst_hash_map.at(stable_hash);
	}

	StablePosition LangElement::getStablePosition() const { return { getHash(), {} }; }
}
