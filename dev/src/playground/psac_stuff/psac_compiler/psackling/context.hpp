#pragma once

#include "ids.hpp"

#include <psackling/namespace.hpp>
#include <psackling/symbol.hpp>
#include <psackling/type.hpp>

#include <base/except/exceptions.hpp>

#include <ranges>
#include <variant>

namespace pg {
	/**
	 * @brief Symbol, type and namespace tables. Pure data — no LLVM state — so it is
	 * cheaply copyable; parallel per-function compilation hands each task its own copy.
	 */
	class Context final {
	private:
		std::vector<ns::NamespaceID>                  namespace_stack;
		base::HashMap<tp::TypeID, tp::TypeInfo>       types;
		base::HashMap<ns::NamespaceID, ns::Namespace> namespaces;
		base::HashMap<sym::SymbolID, sym::Symbol>     symbols;

	public:
		Context():
			  root_ns_id([this]() {
				  // Instantiate the top level namespace, it will be used for all top level declarations
				  ns::NamespaceID id = ns::NamespaceID::next();
				  namespaces.insertOrAssign(
					  id,
					  ns::Namespace{
						  .name = base::StrID("$"), .id = id, .parent_namespace = {}, .symbols = {} }
				  );
				  namespace_stack.push_back(id);
				  return id;
			  }()),
			  bool_type_id(newType(base::StrID("bool"), tp::Boolean{})),
			  void_type_id(newType(base::StrID("void"), tp::Void{})),
			  incomplete_type_id(newType(base::StrID("$incomplete"), tp::Incomplete{})),
			  namespace_type_id(newType(base::StrID("$namespace"), tp::Namespace{})),
			  i64_type_id(newType(base::StrID("i64"), tp::Integral{ .size = Bytes(8) })) {}

		ns::Namespace& currNs() { return namespaces[namespace_stack.back()]; }

		sym::SymbolID declareSymbol(base::StrID name, sym::SymbolKind kind) {
			return insertSymbol(name, std::move(kind), incomplete_type_id);
		}

		sym::SymbolID insertSymbol(base::StrID name, sym::SymbolKind kind, tp::TypeID type_id) {
			sym::SymbolID id = sym::SymbolID::next();
			if (currNs().symbols.contains(name)) {
				throw std::runtime_error(
					"Symbol " + name.str() + " already declared in namespace " + currNs().name.str()
				);
			}
			currNs().symbols.insertOrAssign(name, id);
			symbols.insertOrAssign(
				id,
				sym::Symbol{ .name = name, .id = id, .type_id = type_id, .kind = std::move(kind) }
			);
			return id;
		}

		/// If a type or function is forward declared, we can use this to update the symbol with the
		/// actual type information once it's available.
		void finalizeSymbol(sym::SymbolID id, tp::TypeID type_id) {
			CORE_ASSERT(symbols.contains(id), "Symbol ID not found");
			CORE_ASSERT(
				std::holds_alternative<sym::TypeSymbol>(symbols[id].kind)
					|| std::holds_alternative<sym::FunctionSymbol>(symbols[id].kind),
				"Can only finalize type and function symbols"
			);
			symbols[id].type_id = type_id;
		}

		// LLVM types are lowered lazily by LlvmLoweringContext on first use, so creating
		// a type here is purely a symbol-table operation.
		tp::TypeID newType(base::StrID name, tp::TypeKind kind) {
			tp::TypeID id = tp::TypeID::next();
			types.insertOrAssign(
				id,
				tp::TypeInfo{
					.name = name, .kind = std::move(kind), .id = id, .namespace_id = currNs().id }
			);
			insertSymbol(name, sym::TypeSymbol{ .type_id = id }, id);
			return id;
		}

		/// Like newType, but lets specifying which namespace the type "lives in"
		/// (used for struct types, whose members live in a child namespace of where the
		/// type name is visible). Does NOT insert a symbol - caller does that in the
		/// outer scope after popping the struct's namespace.
		tp::TypeID newTypeInScope(base::StrID name, tp::TypeKind kind, ns::NamespaceID owner_ns) {
			tp::TypeID id = tp::TypeID::next();
			types.insertOrAssign(
				id,
				tp::TypeInfo{
					.name = name, .kind = std::move(kind), .id = id, .namespace_id = owner_ns }
			);
			return id;
		}

		ns::NamespaceID newNamespace(base::StrID name) {
			ns::NamespaceID id = ns::NamespaceID::next();
			namespaces.insertOrAssign(
				id,
				ns::Namespace{
					.name = name, .id = id, .parent_namespace = currNs().id, .symbols = {} }
			);
			insertSymbol(name, sym::NamespaceSymbol{ .namespace_id = id }, namespace_type_id);
			return id;
		}

		/// Lookup restricted to a single namespace — used for qualified access (`ns::member`),
		/// which must not fall back to parent scopes.
		sym::SymbolID lookupInNamespace(ns::NamespaceID ns_id, base::StrID name) {
			ns::Namespace& ns = namespaces[ns_id];
			if (ns.symbols.contains(name)) return ns.symbols[name];
			throw std::runtime_error(
				"Symbol " + name.str() + " not found in namespace " + ns.name.str()
			);
		}

		/// Builds the namespace-qualified name for a symbol declared in the current scope,
		/// e.g. `add` inside `namespace math` becomes `math.add`. Used as the LLVM link name
		/// so same-named functions in different namespaces don't collide.
		base::StrID qualifyName(base::StrID name) {
			std::string qualified;
			for (ns::NamespaceID id: namespace_stack) {
				if (id == root_ns_id) continue;
				qualified += namespaces[id].name.str();
				qualified += '.';
			}
			qualified += name.str();
			return base::StrID(qualified);
		}

		sym::SymbolID lookupInScopeAndParents(ns::NamespaceID ns_id, base::StrID name) {
			ns::Namespace& ns = namespaces[ns_id];
			if (ns.symbols.contains(name)) return ns.symbols[name];
			if (ns.parent_namespace.has_value())
				return lookupInScopeAndParents(*ns.parent_namespace, name);
			throw std::runtime_error(
				"Symbol " + name.str() + " not found in namespace " + ns.name.str()
				+ " or any of its parents"
			);
		}

		tp::TypeID lookupTypeInScopeAndParents(ns::NamespaceID ns_id, base::StrID name) {
			sym::SymbolID sym_id = lookupInScopeAndParents(ns_id, name);
			if (!std::holds_alternative<sym::TypeSymbol>(symbols[sym_id].kind)) {
				throw std::runtime_error(
					"Symbol " + name.str() + " found in namespace " + namespaces[ns_id].name.str()
					+ " but is not a type"
				);
			}
			return symbols[sym_id].type_id;
		}

		ns::NamespaceID pushNamespace(base::StrID name) {
			ns::NamespaceID id = newNamespace(name);
			namespace_stack.push_back(id);
			return id;
		}

		/// Enter an already-existing namespace (e.g. one created by an earlier pass).
		void enterNamespace(ns::NamespaceID id) { namespace_stack.push_back(id); }

		void popNamespace() { namespace_stack.pop_back(); }

		tp::TypeInfo& getTypeInfo(tp::TypeID id) { return types[id]; }

		ns::Namespace& getNamespace(ns::NamespaceID id) { return namespaces[id]; }

		sym::Symbol& getSymbol(sym::SymbolID id) { return symbols[id]; }

		/// All GlobalVariableSymbols in declaration order; the globals object file
		/// defines exactly these.
		std::vector<sym::SymbolID> global_variables;

		// Not const so Context stays copy-assignable: incremental compilation refreshes a
		// long-lived Context in place (PSAC read closures hold a pointer to it).
		ns::NamespaceID root_ns_id;
		tp::TypeID      bool_type_id;
		tp::TypeID      void_type_id;
		tp::TypeID      incomplete_type_id;
		tp::TypeID      namespace_type_id;
		tp::TypeID      i64_type_id;
	};
}
