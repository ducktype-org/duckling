// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lang_parser_element.hpp"

#include "../elements/includes/basic.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace pst {

	base::Optional<AccessLocked<LangElement>> LangElement::getParent() const {
		// The parent of a non-Full element belongs to the same unlock group (see AccessLockedState).
		if (parent.has_value() && unlock_mode != UnlockMode::Full)
			return AccessLocked<LangElement>(parent->ref, AccessLockedState::Unlocked);
		return parent;
	}

	SourcePositionLocked LangElement::getSourcePosition() const { return { source_position }; }

	void LangElement::unlockWholeSubTree(query::Context& ctx) const {
		for (const auto& child: viewChildren())
			if (auto unlocked = child.unlockOpt(ctx)) unlocked.value()->unlockWholeSubTree(ctx);
	}

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
					hashing::PathHash child_path(getElementPathHash(), named_child.name);
					named_child.element->calcElementPathHash(child_path);
				}
			}
		}
	}

	void LangElement::calcOrderedListChildPath(
		std::vector<AccessInternalAnonymous<Stmt>>& statements, const hashing::PathHash& path
	) {
		auto                          no_symbol_path  = hashing::PathHash(path, "no_symbol");
		usize                         no_symbol_count = 0;
		auto                          by_symbol_path  = hashing::PathHash(path, "by_symbol");
		base::Map<base::StrID, usize> by_symbol_count;
		usize                         symbol_count      = 0;
		auto                          transparent_path  = hashing::PathHash(path, "transparent");
		usize                         transparent_count = 0;

		hashing::PathHash id_path = no_symbol_path;

		for (auto& stmt: statements) {
			base::StrID symbol;
			switch (stmt.internal()->isDeclaration()) {
			case DeclKind::None:
				id_path
					= hashing::PathHash(no_symbol_path, hashing::PathHash::Index{ no_symbol_count });
				calcChildPath(stmt, id_path);
				no_symbol_count++;
				break;
			case DeclKind::Symbol:
				symbol = stmt.internal()->getInternalSymbolName().value();
				if (by_symbol_count.atMaybe(symbol)) {
					symbol_count = by_symbol_count[symbol];
				} else {
					by_symbol_count.put(symbol);
					symbol_count = 0;
				}
				id_path = hashing::PathHash(
					by_symbol_path, std::format("{}[{}]", symbol.str(), symbol_count)
				);
				calcChildPath(stmt, id_path);
				by_symbol_count[symbol]++;
				break;
			case DeclKind::Transparent:
				id_path = hashing::PathHash(
					transparent_path, hashing::PathHash::Index{ transparent_count }
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

		// Can be used to turn on unstable hashing for testing purposes.
		// hash = getID().asInt();
	}

	void LangElement::putInPSTHashHashMapRecursive() {
		putInPSTHashHashMap();
		for (auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) { el->putInPSTHashHashMapRecursive(); }
				variant_case(InternalNamedChild, el) { el.element->putInPSTHashHashMapRecursive(); }
			}
		}
	}

	void LangElement::putInPSTHashHashMap() {
		pst_hash_map.putOrAssign(hash.value(), AccessLocked<LangElement>(CRef<LangElement>(this)));
		// Unlock group hashes are PST access side input keys, map them to the group roots.
		if (unlock_group_hash.has_value())
			pst_hash_map.putOrAssign(
				unlock_group_hash.value(), AccessLocked<LangElement>(CRef<LangElement>(this))
			);
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
				variant_case(InternalChild, el) { el->calcSignature(partial_hash); }
				variant_case(InternalNamedChild, el) { el.element->calcSignature(partial_hash); }
				variant_case(SubToken, el) {}
				variant_default { CORE_PANIC("Unhandled variant case"); }
			}
		}
		addToHash(partial_hash, "hash_end");
	}

	void LangElement::signGenerated(const HashType& signature) {
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

	void LangElement::calcUnlockGroups(std::vector<HashType>& group_hashes) {
		CORE_ASSERT(unlock_mode == UnlockMode::Full, "Unlock group root has to be Full");
		HashAlg unused;
		calcUnlockGroupsRecursive(this, unused, group_hashes);
	}

	void LangElement::calcUnlockGroupsRecursive(
		CRef<LangElement> group_root, HashAlg& group_hash, std::vector<HashType>& group_hashes
	) {
		auto for_each_child = [&](auto fun) {
			for (auto& sub_el: sub_elements) {
				variant_match(sub_el) {
					variant_case(InternalChild, el) { fun(el); }
					variant_case(InternalNamedChild, el) { fun(el.element); }
					variant_case(SubToken, el) {}
					variant_default { CORE_PANIC("Unhandled variant case"); }
				}
			}
		};

		switch (unlock_mode) {
		case UnlockMode::Full: {
			unlock_group_root = this;
			HashAlg my_group_hash;
			addToHash(my_group_hash, "unlock_group");
			addToHash(my_group_hash, hash->data);
			for_each_child([&](Ref<LangElement> child) {
				child->calcUnlockGroupsRecursive(this, my_group_hash, group_hashes);
			});
			addToHash(my_group_hash, "unlock_group_end");
			unlock_group_hash = my_group_hash.finalize();
			group_hashes.push_back(unlock_group_hash.value());
			return;
		}
		case UnlockMode::Transparent:
			unlock_group_root = group_root;
			addToHash(group_hash, hash->data);
			for_each_child([&](Ref<LangElement> child) {
				child->calcUnlockGroupsRecursive(group_root, group_hash, group_hashes);
			});
			addToHash(group_hash, "element_end");
			return;
		case UnlockMode::Generated:
			CORE_PANIC("Unlock groups are not calculated for generated PSTs");
		}
		CORE_UNREACHABLE();
	}

	HashType LangElement::getUnlockGroupHash() const {
		auto root = unlock_group_root.toOpt();
		CORE_ASSERT(root.has_value(), "Unlock group not calculated for this: " + elementType());
		return root.value()->unlock_group_hash.value();
	}

	std::vector<CRef<LangElement>> LangElement::getUnlockGroupElements() const {
		std::vector<CRef<LangElement>> out{ this };
		for (usize i = 0; i < out.size(); i++) {
			for (const auto& child: out[i]->viewChildren()) {
				auto el = child.illegalAccess();
				if (el.has_value() && el.value()->getUnlockMode() == UnlockMode::Transparent)
					out.push_back(&*el.value());
			}
		}
		return out;
	}

	void LangElement::markGeneratedRecursive() {
		unlock_mode = UnlockMode::Generated;
		for (auto& sub_el: sub_elements) {
			variant_match(sub_el) {
				variant_case(InternalChild, el) { el->markGeneratedRecursive(); }
				variant_case(InternalNamedChild, el) { el.element->markGeneratedRecursive(); }
				variant_case(SubToken, el) {}
				variant_default { CORE_PANIC("Unhandled variant case"); }
			}
		}
	}

	MCRef<LangElement> LangElement::getInputOrigin() const {
		if (unlock_mode != UnlockMode::Generated) return this;
		if (auto origin = clone_origin.toOpt()) return origin.value()->getInputOrigin();
		if (parent.has_value()) {
			if (auto parent_el = parent->illegalAccess())
				return parent_el.value()->getInputOrigin();
			return nullptr;
		}
		if (additional_root_data.has_value()) {
			if (auto macro = std::get_if<AdditionalRootData::MacroExpansionParent>(
					&additional_root_data->pst_parent
				))
				if (auto expand = macro->expand_element.illegalAccess())
					return expand.value()->getInputOrigin();
		}
		return nullptr;
	}

	query::QueryStableHash LangElement::getInputOriginHash(query::QueryStableHash stable_hash) {
		auto element = pst_hash_map.atMaybeCopy(stable_hash);
		if (!element) return stable_hash;
		auto el = element.value().illegalAccess();
		if (!el) return stable_hash;
		if (auto origin = el.value()->getInputOrigin().toOpt()) return origin.value()->getHash();
		return stable_hash;
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
			setLastToken(el->getSourcePosition().illegalAccess());
		}
	}

	void LangElement::addNamedChild(const std::string& name, MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			sub_elements.emplace_back(InternalNamedChild{ .name = name, .element = opt.value() });
			setLastToken(el->getSourcePosition().illegalAccess());
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

	void LangElement::setAdditionalRootData(AdditionalRootData data) {
		CORE_ASSERT(!additional_root_data.has_value(), "Additional root data already set");
		CORE_ASSERT(!parent.has_value(), "Only root elements can have additional root data");
		additional_root_data.emplace(std::move(data));
	}

	void LangElement::acceptVisitor(PstVisitor&) const {
		CORE_PANIC("PstVisitor not supported for " + elementType());
	}

	concurrent::ConHashMap<query::QueryStableHash, AccessLocked<LangElement>>
		LangElement::pst_hash_map{};

	namespace {
		void (*unknown_stable_hash_handler)() = nullptr;
	}

	void LangElement::setUnknownStableHashHandler(void (*handler)()) {
		unknown_stable_hash_handler = handler;
	}

	AccessLocked<LangElement> LangElement::getByStableHash(query::QueryStableHash stable_hash) {
		if (auto found = pst_hash_map.atMaybeCopy(stable_hash)) return found.value();
		if (unknown_stable_hash_handler) unknown_stable_hash_handler();
		auto found = pst_hash_map.atMaybeCopy(stable_hash);
		CORE_ASSERT(found.has_value(), "Invalid stable hash");
		return found.value();
	}

	dia::StablePosition LangElement::getStablePosition() const {
		return {
			LangElement::getActiveSourcePosition,
			LangElement::getActiveSourcePositionIllegalAccess,
			getHash(),
			{},
		};
	}

	dia::SourcePosition LangElement::getActiveSourcePositionIllegalAccess(
		const dia::StablePosition& pos
	) {
		auto first_pos = LangElement::getByStableHash(pos.begin_node)
		                     .illegalAccess()
		                     .value()
		                     ->getSourcePosition()
		                     .illegalAccess();

		if (pos.end_node.has_value()) {
			auto last_pos = LangElement::getByStableHash(pos.end_node.value())
			                    .illegalAccess()
			                    .value()
			                    ->getSourcePosition()
			                    .illegalAccess();
			return dia::SourcePosition::merge(first_pos, last_pos);
		} else {
			return first_pos;
		}
	}

	dia::SourcePosition LangElement::getActiveSourcePosition(
		query::Context& ctx, const dia::StablePosition& pos
	) {
		auto first_pos = LangElement::getByStableHash(pos.begin_node)
		                     .unlock(ctx)
		                     ->getSourcePosition()
		                     .unlock(ctx);

		if (pos.end_node.has_value()) {
			auto last_pos = LangElement::getByStableHash(pos.end_node.value())
			                    .unlock(ctx)
			                    ->getSourcePosition()
			                    .unlock(ctx);
			return dia::SourcePosition::merge(first_pos, last_pos);
		} else {
			return first_pos;
		}
	}

	MBox<LangElement> LangElement::clone() const {
		auto clone    = cloneElement();
		u64  my_count = 0, clone_count = 0;
		IF_BUILD_TYPE_DEV(for ([[maybe_unused]] auto& ref : viewChildren()) my_count++;
		                  for ([[maybe_unused]] auto& ref : clone->viewChildren()) clone_count++;
		                  CORE_ASSERT(my_count == clone_count, "Not all children cloned."););
		return clone;
	}
}
