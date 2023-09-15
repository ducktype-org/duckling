#include "lookup_result.hpp"
#include <base/exceptions.hpp>
#include <hir/analysis_state.hpp>
#include "symbol.hpp"
#include <iostream>

namespace symtable {

	SymbolChain deAliasSymbolChain(const SymbolChain& chain) {
		// @TODO: this does not handle non-unique symbols (overloaded)
		SymbolChain out;
		std::cerr << "     dealiasing... \n";
		for (usize i = 0; i < chain.size(); i++) {
			auto de_aliased = chain[i]->getUniqueDeAlias();
			std::cerr << "           ";
			dprintSymbolChain(de_aliased, std::cerr);
			std::cerr << "\n";
			out.insert(out.end(), de_aliased.begin(), de_aliased.end());
		}
		return out;
	}

	bool LookupResult::isEmpty() const {
		return leaves.empty() and children.empty();
	}

	bool LookupResult::isSingle() const {
		if (leaves.size() == 1 and children.size() == 0) return true;
		if (leaves.size() == 0 and children.size() == 1 and children[0].inner.isSingle()) return true;
		return false;
	}

	SymbolChain LookupResult::getAsSingleReverse() {
		RIFT_ASSERT(isSingle(), "getAsSingle on non-single lookup result");
		if (leaves.size() == 1) {
			return leaves;
		}
		else if (children.size() == 1) {
			auto single_from_child = children[0].inner.getAsSingleReverse();
			single_from_child.push_back(children[0].node);
			return single_from_child;
		}
		else {
			RIFT_PANIC("getAsSingle failed");
		}
	}

	SymbolChain LookupResult::getAsSingle() {
		RIFT_ASSERT(isSingle(), "getAsSingle on non-single lookup result");
		auto res = getAsSingleReverse();
		std::reverse(res.begin(), res.end());
		return res;
	}

	void LookupResult::insert(LookupResult&& other) {
		leaves.insert(leaves.end(), 
			std::make_move_iterator(other.leaves.begin()), 
			std::make_move_iterator(other.leaves.end()));

		children.insert(children.end(), 
			std::make_move_iterator(other.children.begin()), 
			std::make_move_iterator(other.children.end()));
	}

	LookupNode LookupResult::toNode(SymbolRef node) & {
		return {node, leaves, children}; 
	}

	LookupNode LookupResult::toNode(SymbolRef node) && {
		return {node, std::move(*this)}; 
	}

	bool ChainLookupResult::isEmpty() const {
		return prefix.empty() && result.isEmpty();
	}

	bool ChainLookupResult::isSingle() const {
		return result.isSingle();
	};
	SymbolChain ChainLookupResult::getAsSingle() {
		auto prefix_copy = prefix;
		auto single = result.getAsSingle();
		prefix_copy.insert(prefix_copy.end(), single.begin(), single.end());
		return prefix_copy;
	}


	// dprints:
	void dprintSymbolChain(const symtable::SymbolChain& chain, std::ostream& out) {
		out << "[";
		for (auto sym: chain) {
			if (sym != nullptr) {
				out << sym->getName().strView();
			}
			else {
				out << "BAD";
			}
			out << " . ";	
		}
		out << "]";
		// out << "\n";
	}

	void LookupResult::dprint(std::ostream& out) {
		out << "Result { ";
		out << "[";
		for (auto leaf: leaves) {
			out << leaf->getName().strView() << ", ";
		}
		out << "] , ";
		out << "Children: [";
		for (auto child: children) {
			child.dprint(out);
		}
		out << "]";
		out << " }";
	}

	void LookupNode::dprint(std::ostream& out) {
		out << "Node{" << node->getName().strView() << ": ";
		inner.dprint(out);
		out << "}";
	}

	void ChainLookupResult::dprint(std::ostream& out) {
		out << "ChainLookupResult: ";
		dprintSymbolChain(prefix, out);
		out << " . ";
		result.dprint(out);
	}
}
