#include "type_info_impl.hpp"
#include <deque>
#include <algorithm>

namespace ts::internal {

	bool checkIfNext(
		ClassInfo class_info, const std::vector<std::deque<ClassInfo>>& C3_linearisation_vector
	) {
		for (const auto& q: C3_linearisation_vector) {
			if (q.empty()) continue;
			auto x = std::find(std::next(q.cbegin()), q.cend(), class_info);
			if (x != q.cend()) return false;
		}
		return true;
	}

	void linearise(
		ClassInfo class_info, std::vector<std::deque<ClassInfo>>& C3_linearisation_vector
	) {
		for (auto& q: C3_linearisation_vector)
			if (!q.empty() && q.front() == class_info) q.pop_front();
	}

	std::optional<ClassInfo>
		lineariseOneStep(std::vector<std::deque<ClassInfo>>& C3_linearisation_vector) {
		bool are_all_empty = true;
		for (auto& q: C3_linearisation_vector) {
			if (q.empty()) continue;
			are_all_empty = false;
			if (checkIfNext(q.front(), C3_linearisation_vector)) {
				ClassInfo result = q.front();
				linearise(q.front(), C3_linearisation_vector);
				return result;
			}
		}
		if (are_all_empty)
			return {};
		else
			throw C3LinearisationException();
	}

	std::deque<ClassInfo>
		mergeLinearisation(std::vector<std::deque<ClassInfo>>& C3_linearisation_vector) {
		std::deque<ClassInfo>    result;
		std::optional<ClassInfo> next_element;
		do {
			next_element = lineariseOneStep(C3_linearisation_vector);
			if (next_element.has_value()) result.push_back(next_element.value());
		} while (next_element.has_value());
		return result;
	}

	ClassInfoImpl::ClassInfoImpl(
		base::StrId                                                   name,
		const std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>& member_types,
		const std::vector<std::pair<ClassInfo, InheritanceTag>>&      inheritance_classes,
		usize                                                         virtualMethods
	):
		  name(name),
		  virt_method_count(virtualMethods) {
		// Initialise parents vector
		std::vector<ClassInfo> parents;
		for (auto [parent_id, inheritance_tag]: inheritance_classes)
			if (!inheritance_tag.is_virtual) parents.push_back(parent_id);

		// Calculate C3 linearisation
		std::vector<std::deque<ClassInfo>> C3_linearisation_vector;
		for (auto [parent_id, inheritance_tag]: inheritance_classes) {
			std::deque<ClassInfo> temp;
			if (inheritance_tag.is_virtual) temp.push_back(parent_id);
			for (auto [ancestor_info, ancestor_offset]:
			     (dynamic_cast<const ClassInfoImpl*>(parent_id.getPimpl()))->virtual_layout) {
				temp.push_back(ancestor_info);
			}
			C3_linearisation_vector.push_back(std::move(temp));
		}

		std::deque<ClassInfo> temp;
		for (auto [parent_id, inheritance_tag]: inheritance_classes)
			if (inheritance_tag.is_virtual) temp.push_back(parent_id);
		C3_linearisation_vector.push_back(std::move(temp));

		std::deque<ClassInfo> linearised_ancestors = mergeLinearisation(C3_linearisation_vector);

		// Set ancestor order
		usize reserved_space = 0;
		for (ClassInfo parent: parents) {
			AncestorData data{ .info = parent, .offset = reserved_space };
			basic_parents.push_back(ancestors_data.size());
			ancestors_data.push_back(data);
			auto parent_pimpl = dynamic_cast<const ClassInfoImpl*>(parent.getPimpl());
			for (AncestorData parent_ancestor: parent_pimpl->ancestors_data) {
				if (!parent_ancestor.last_virtual_ancestor.has_value()) {
					parent_ancestor.offset += reserved_space;
					ancestors_data.push_back(parent_ancestor);
				}
			}
			reserved_space += parent.getBaseSize();
		}

		// Get members data for our parents
		for (usize parent_position: basic_parents) {
			ClassInfo parent_info    = ancestors_data[parent_position].info;
			usize     parent_offset  = ancestors_data[parent_position].offset;
			auto      ancestor_pimpl = dynamic_cast<const ClassInfoImpl*>(parent_info.getPimpl());

			for (MemberData member_data: ancestor_pimpl->members_data) {
				if (!member_data.last_virtual_ancestor.has_value()) {
					member_data.offset += parent_offset;

					members_data.push_back(member_data);
				}
			}
		}

		// Add our members, reserve the space for them
		for (auto [desc, symbol]: member_types) {
			MemberData data{ .symbol = symbol, .desc = desc, .offset = reserved_space };
			direct_members.push_back(members_data.size());
			members_data.push_back(data);
			reserved_space += 0; //desc.getType().getSize();
		}

		if (virt_method_count > 0 || !linearised_ancestors.empty()) reserved_space += POINTER_SIZE;

		base_size = reserved_space;

		// Reserve the space for our virtual ancestors
		for (ClassInfo ancestor: linearised_ancestors) {
			virtual_layout.emplace_back(ancestor, reserved_space);
			reserved_space += ancestor.getBaseSize();
		}

		// Add the ancestor data about our virtual ancestors
		for (ClassInfo ancestor: linearised_ancestors) {
			AncestorData data{ .info = ancestor, .offset = 0, .last_virtual_ancestor = ancestor };
			ancestors_data.push_back(data);
			auto parent_pimpl = dynamic_cast<const ClassInfoImpl*>(ancestor.getPimpl());
			for (AncestorData parent_ancestor: parent_pimpl->ancestors_data) {
				if (!parent_ancestor.last_virtual_ancestor.has_value()) {
					parent_ancestor.last_virtual_ancestor = ancestor;
					ancestors_data.push_back(parent_ancestor);
				}
			}
		}


		for (ClassInfo virtual_ancestor: virtualAncestors()) {
			for (MemberData member_data: virtual_ancestor.members()) {
				if (!member_data.last_virtual_ancestor.has_value()) {
					member_data.last_virtual_ancestor = virtual_ancestor;

					members_data.push_back(member_data);
				}
			}
		}

		// Fill indexes
		for (usize i = 0; i < ancestors_data.size(); i++) {
			AncestorData& ancestor_data = ancestors_data[i];
			if (!ancestors_positions.contains(ancestor_data.info))
				ancestors_positions.put(ancestor_data.info, {});
			ancestors_positions[ancestor_data.info].push_back(i);
		}
		for (usize i = 0; i < members_data.size(); i++) {
			MemberData& member_data = members_data[i];
			if (!members_positions.contains(member_data.symbol))
				members_positions.put(member_data.symbol, {});
			members_positions[member_data.symbol].push_back(i);
		}

		// Set the TypeInfo size variable, previously set to 0
		size = reserved_space;

		// @TODO: classes should probably know their name.
		representation = "Class: " + name.str();
	}

	usize ClassInfoImpl::getBaseSize() const { return base_size; }

	const std::vector<AncestorData>& ClassInfoImpl::allAncestors() const { return ancestors_data; }

	const std::vector<AncestorData> ClassInfoImpl::basicParents() const {
		std::vector<AncestorData> result;
		result.reserve(basic_parents.size());
		for (usize parent_position: basic_parents)
			result.push_back(ancestors_data[parent_position]);
		return result;
	}

	const std::vector<ClassInfo> ClassInfoImpl::virtualAncestors() const {
		std::vector<ClassInfo> virtual_ancestors;
		virtual_ancestors.reserve(virtual_layout.size());
		for (auto [id, offset]: virtual_layout) virtual_ancestors.push_back(id);
		return virtual_ancestors;
	}

	const std::vector<MemberData> ClassInfoImpl::members() const {
		std::vector<MemberData> result;
		result.reserve(direct_members.size());
		for (auto position: direct_members) result.push_back(members_data[position]);
		return result;
	}

	const std::vector<MemberData>& ClassInfoImpl::allMembers() const { return members_data; }

	// Gets the offset of (and some data about) our member
	// We return it in case of no ambiguity
	MemberInfo ClassInfoImpl::getMemberInfo(symtable::SymbolId symbol) const {
		if (!members_positions.contains(symbol)) return { .result_type = ResultType::NoResult };
		const std::vector<usize>& possible_positions = members_positions[symbol];

		if (possible_positions.size() > 1) return { .result_type = ResultType::Ambiguous };

		const MemberData& data = members_data[possible_positions[0]];
		if (data.last_virtual_ancestor.has_value()) {
			return { .last_virtual_ancestor = data.last_virtual_ancestor,
				     .desc                  = data.desc,
				     .start_offset          = data.offset,
				     .end_offset            = data.offset + 0, // data.desc.getType().getSize(),
				     .result_type           = ResultType::Virtual };
		}
		return { .desc         = data.desc,
			     .start_offset = data.offset,
			     .end_offset   = data.offset + 0, // data.desc.getType().getSize(),
			     .result_type  = ResultType::Standard };
	}

	// Gets the offset of (and some data about) our member
	// Uses the hint to look what inheritance path to go through
	MemberInfo
		ClassInfoImpl::getMemberInfo(symtable::SymbolId symbol, std::vector<ClassInfo> hint) const {
		AncestorInfo ancestor_info = getAncestorInfo(hint);

		const ClassInfoImpl* ancestor_to_look_from = nullptr;
		if (hint.empty())
			ancestor_to_look_from = this;
		else
			ancestor_to_look_from = dynamic_cast<const ClassInfoImpl*>(hint.back().getPimpl());

		MemberInfo member_info = ancestor_to_look_from->getMemberInfo(symbol);

		return ancestor_info ^ member_info;
	}

	// Looks for the offset of (and some data about) our specific ancestor
	AncestorInfo ClassInfoImpl::getAncestorInfo(ClassInfo ancestor_id) const {
		if (ancestor_id.getPimpl() == this) {
			return {
				.start_offset = 0,
				.end_offset   = base_size,
				.result_type  = ResultType::Standard,
			};
		}

		if (!ancestors_positions.contains(ancestor_id))
			return { .result_type = ResultType::NoResult };

		const std::vector<usize>& possible_positions = ancestors_positions[ancestor_id];

		if (possible_positions.size() > 1) return { .result_type = ResultType::Ambiguous };

		const AncestorData& data = ancestors_data[possible_positions[0]];
		if (data.last_virtual_ancestor.has_value()) {
			return { .last_virtual_ancestor = data.last_virtual_ancestor,
				     .start_offset          = data.offset,
				     .end_offset            = data.offset + data.info.getBaseSize(),
				     .result_type           = ResultType::Virtual };
		}
		return { .start_offset = data.offset,
			     .end_offset   = data.offset + data.info.getBaseSize(),
			     .result_type  = ResultType::Standard };
	}

	// Looks for the offset of (and some data about) our specific ancestor
	// Uses the other ancestor ids as the inheritance path hint
	AncestorInfo ClassInfoImpl::getAncestorInfo(const std::vector<ClassInfo>& ancestor_ids) const {
		AncestorInfo result
			= { .start_offset = 0, .end_offset = base_size, .result_type = ResultType::Standard };

		const ClassInfoImpl* last_ancestor = this;

		for (auto ancestor_id: ancestor_ids) {
			result ^= last_ancestor->getAncestorInfo(ancestor_id);
			last_ancestor = dynamic_cast<const ClassInfoImpl*>(ancestor_id.getPimpl());
		}
		return result;
	}

	usize ClassInfoImpl::getVirtualAncestorOffset(ClassInfo ancestor_id) const {
		for (auto [checked_id, ancestor_offset]: virtual_layout)
			if (ancestor_id == checked_id) return ancestor_offset;

		throw base::LogicError("This is not a virtual parent of this class");
	}

	std::vector<std::pair<ClassInfo, usize>>
		ClassInfoImpl::getVirtualAncestorTable(ClassInfo ancestor_id) const {
		return getVirtualAncestorTable((std::vector<ClassInfo>){ ancestor_id });
	}

	std::vector<std::pair<ClassInfo, usize>>
		ClassInfoImpl::getVirtualAncestorTable(std::vector<ClassInfo> ancestor_ids) const {
		// @TODO: Fix this function
		std::vector<std::pair<ClassInfo, usize>> result;

		const ClassInfoImpl* parent_ptr = nullptr;
		if (!ancestor_ids.empty())
			parent_ptr = (dynamic_cast<const ClassInfoImpl*>(ancestor_ids.back().getPimpl()));
		else
			parent_ptr = this;

		auto  clueless_parent_info = getAncestorInfo(ancestor_ids);
		usize parent_offset        = 0;

		switch (clueless_parent_info.result_type) {
			using enum ts::ResultType;
		case Standard:
			parent_offset = clueless_parent_info.start_offset.value();
			break;
		case Virtual:
			parent_offset
				= clueless_parent_info.start_offset.value()
			    + getVirtualAncestorOffset(clueless_parent_info.last_virtual_ancestor.value());
			break;
		case Ambiguous:
			throw base::LogicError(
				"Not enough info to create such vtable - this class has multiple such ancestors"
			);
		default:
			throw base::LogicError(
				"Tried to get an invalid vtable instance - this class has no such ancestor"
			);
		}

		for (auto [parent_virt_ancestor, parent_virt_ancestor_offset]: parent_ptr->virtual_layout) {
			// @TODO: Consider throwing virtual ancestors here in a set here temporarily
			// This could reduce this function to O(n*log(n)), but there should be very few virtual
			// ancestors, so may be unnecessary.
			usize virt_ancestor_offset = getVirtualAncestorOffset(parent_virt_ancestor);

			result.emplace_back(parent_virt_ancestor, virt_ancestor_offset - parent_offset);
		}

		return result;
	}

	usize ClassInfoImpl::getVtablePtrOffset() const {
		if (virtual_layout.empty() && virt_method_count == 0)
			throw base::LogicError("Tried to get a vtable of a class without one");
		return base_size - POINTER_SIZE;
	}

	usize ClassInfoImpl::getVtableSize() const {
		// @TODO: Possibly add size multipliers
		return virtual_layout.size() + virt_method_count;
	}

	usize ClassInfoImpl::getVtablePositionOf(ts::ClassInfo ancestor) const {
		// @TODO: Maybe add error handling?
		for (usize i = 0; i < virtual_layout.size(); i++) {
			auto [info, offset] = virtual_layout[i];
			if (info == ancestor) return i;
		}
		throw base::LogicError(
			"Tried to get a vtable position of someone who is not our virtual ancestor"
		);
	}

	// ClassInfoImpl::~ClassInfoImpl() = default;
}
