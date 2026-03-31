#pragma once

#include <string_id/string_id.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <utility>

namespace loader::compiler {
	template<typename VarT, typename ValT>
	class LocalStackDbBuilder;

	template<typename VarT, typename ValT>
	class LocalStackDatabase;

	STRONG_TYPEDEF_INT(StackStateID, u64);

	template<typename VarT, typename ValT>
	class LocalStackDatabase {
		friend LocalStackDbBuilder<VarT, ValT>;

		//// Vals - immutable parts (usually decltype info)
		using ValNodeID = u64;

		struct Lifetime {
			usize deinit_idx                         = 0;
			usize init_idx                           = 0;
			auto  operator<=>(const Lifetime&) const = default;
		};

		struct ValNode {
			Lifetime  lifetime{};
			ValNodeID prev = 0;
			usize     size = 0;
			ValT      info{};
		};

		using ValsMap = std::map<Lifetime, ValNodeID>;

		// Vars - values which can be changed in
		using VarNodeID = u64;

		struct VarNode {
			VarNodeID left;
			VarNodeID rght;
			auto      operator<=>(const VarNode&) const = default;
		};

		struct StackStateEntry {
			ValNodeID val_state;
			VarNodeID var_state;
		};

		base::HashMap<ValNodeID, ValNode>   val_entries;
		base::HashMap<base::StrID, ValsMap> name_to_decl_info;

		base::HashMap<VarNodeID, VarNode> var_entries;
		base::HashMap<VarNodeID, usize>   root_heights;
		base::HashMap<VarNodeID, VarT>    leaf_values;
		std::map<VarNode, VarNodeID>      children_to_node;

		base::HashMap<StackStateID, StackStateEntry> states;

		LocalStackDatabase(
			decltype(val_entries)       values_entry,
			decltype(name_to_decl_info) name_to_decl_info,
			decltype(var_entries)       var_entries,
			decltype(root_heights)      root_height,
			decltype(leaf_values)       leaf_values,
			decltype(children_to_node)  children_to_node,
			decltype(states)            states
		):
			  val_entries(std::move(values_entry)),
			  name_to_decl_info(std::move(name_to_decl_info)),
			  var_entries(std::move(var_entries)),
			  root_heights(std::move(root_height)),
			  leaf_values(std::move(leaf_values)),
			  children_to_node(std::move(children_to_node)),
			  states(std::move(states)) {}

	public:
		LocalStackDatabase() = default;
	};

	template<typename VarT, typename ValT>
	class LocalStackDbBuilder {
		using Prod = LocalStackDatabase<VarT, ValT>;

		using ValNodeID = Prod::ValNodeID;
		using Lifetime  = Prod::Lifetime;
		using ValNode   = Prod::ValNode;
		using ValsMap   = Prod::ValsMap;

		using VarNodeID       = Prod::VarNodeID;
		using VarNode         = Prod::VarNode;
		using StackStateEntry = Prod::StackStateEntry;

		using ValEntries   = decltype(Prod::val_entries);
		using NameDeclInfo = decltype(Prod::name_to_decl_info);
		using VarEntries   = decltype(Prod::var_entries);
		using RootHeights  = decltype(Prod::root_heights);
		using LeafValues   = decltype(Prod::leaf_values);
		using ChilToNodeId = decltype(Prod::children_to_node);
		using States       = decltype(Prod::states);

		ValEntries   val_entries;
		NameDeclInfo name_to_decl_info;

		VarEntries   var_entries;
		RootHeights  root_heights;
		LeafValues   leaf_values;
		ChilToNodeId children_to_node;

		States states;

	public:
		void push(StackStateID state, base::StrID name, ValT val, VarT var) {
			
		}
		
	};
}
