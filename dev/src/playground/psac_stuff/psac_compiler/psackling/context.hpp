#pragma once

#include "ids.hpp"
#include "psackling/type.hpp"
#include "psackling/symbol.hpp"
#include "psackling/namespace.hpp"

namespace pg {
	class Context final {
	public:
		Context():
			  root_ns_id([this]() {
				  // Instantiate the top level namespace, it will be used for all top level declarations
				  ns::NamespaceID id = ns::NamespaceID::next();
				  namespaces[id]     = {
					  .name = base::StrID("$"), .id = id, .parent_namespace = {}, .symbols = {}
				  };
				  namespace_stack.push_back(id);
				  return id;
			  }()),
			  bool_type_id(newType(base::StrID("$bool"), tp::Boolean{})),
			  incomplete_type_id(newType(base::StrID("$incomplete"), tp::Incomplete{})),
			  void_type_id(newType(base::StrID("$void"), tp::Void{})) {}

		ns::Namespace& currNs() { return namespaces[namespace_stack.back()]; }

		sym::SymbolID declareSymbol(base::StrID name, sym::SymbolKind kind) {
			return pushSymbol(name, std::move(kind), incomplete_type_id);
		}

		sym::SymbolID pushSymbol(base::StrID name, sym::SymbolKind kind, tp::TypeID type_id) {
			sym::SymbolID id = sym::SymbolID::next();
			if (currNs().symbols.contains(name)) {
				throw std::runtime_error(
					"Symbol " + name.str() + " already declared in namespace " + currNs().name.str()
				);
			}
			currNs().symbols[name] = id;
			symbols[id] = { .name = name, .id = id, .type_id = type_id, .kind = std::move(kind) };
			return id;
		}

		tp::TypeID newType(base::StrID name, tp::TypeKind kind) {
			tp::TypeID id = tp::TypeID::next();
			types[id]
				= { .name = name, .kind = std::move(kind), .id = id, .namespace_id = currNs().id };
			return id;
		}

        ns::NamespaceID newNamespace(base::StrID name) {
            ns::NamespaceID id = ns::NamespaceID::next();
            namespaces[id] = { .name = name, .id = id, .parent_namespace = currNs().id, .symbols = {} };
            return id;
        }

	private:
		const ns::NamespaceID root_ns_id;
		const tp::TypeID      bool_type_id;
		const tp::TypeID      incomplete_type_id;
		const tp::TypeID      void_type_id;

		std::vector<ns::NamespaceID> namespace_stack;  /// Stack of namespaces we're currently in

		base::HashMap<tp::TypeID, tp::TypeInfo>       types;
		base::HashMap<ns::NamespaceID, ns::Namespace> namespaces;
		base::HashMap<sym::SymbolID, sym::Symbol>     symbols;
	};
}
