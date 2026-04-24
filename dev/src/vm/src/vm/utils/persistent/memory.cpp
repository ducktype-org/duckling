#include "memory.hpp"

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/types/ints.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include "vm/utils/persistent/tree.hpp"
#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

namespace vm::persistent {

	std::vector<std::pair<usize, usize>> Memory::toVec(MemoryStateID state) const {
		auto root = validateInput(state);

		if (root == SegmentTree::EMPTY) return {};

		std::vector<std::pair<usize, usize>> ans = {};

		auto [left_idx, _] = getRange(root);
		auto path          = getPathTo(root, left_idx);

		do {
			auto idx = path.idx;
			auto val = getValue(path.trace.at(0));
			ans.emplace_back(idx, val);
		} while (path.moveToValid(SegmentTree::Dir::Right, 0));

		return ans;
	}

	Memory::diffResT Memory::getDiff(MemoryStateID state_1, MemoryStateID state_2) const {
		auto [root_1, root_2] = validateInput(state_1, state_2);

		using helper = std::function<void(ID, usize)>;

		std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>> ans = {};

		helper add_left = [&, mem = this](ID node, usize) {
			auto vec = mem->toVec(toState(node));
			for (auto [idx, val]: vec) ans.emplace_back(idx, val, std::nullopt);
		};

		helper add_right = [&, mem = this](ID node, usize) {
			auto vec = mem->toVec(toState(node));
			for (auto [idx, val]: vec) ans.emplace_back(idx, std::nullopt, val);
		};

		detail::SegmentTree::rebuildFromTwo(
			root_1,
			root_2,
			MergeBuilder<void>{
				.only_1   = add_left,
				.only_2   = add_right,
				.the_same = [](ID, usize) {},
				.confilicts
				= [&](usize idx, usize val_1, usize val_2) { ans.emplace_back(idx, val_1, val_2); },
			}
		);

		return ans;
	}

	MemoryStateID Memory::merge(MemoryStateID state_1, MemoryStateID state_2, ConflictPolicy policy) {
		auto [root_1, root_2] = validateInput(state_1, state_2);

		std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>> ans = {};

		auto new_root = detail::SegmentTree::rebuildFromTwo(
			root_1,
			root_2,
			MergeBuilder<ID>{
				.only_1   = [](ID id, usize) { return id; },
				.only_2   = [](ID id, usize) { return id; },
				.the_same = [](ID id, usize) { return id; },
				.confilicts =
					[&](usize idx, usize val_1, usize val_2) {
						match_optional(policy(idx, val_1, val_2)) {
							opt_some(val) { return nodeFromIdxVar(idx, val); }
							opt_none { return detail::SegmentTree::EMPTY; }
						}
						CORE_UNREACHABLE();
					},
			}
		);

		return toState(new_root);
	}

	MemoryStateID Memory::slice(MemoryStateID state, usize left_idx, usize right_idx) {
		auto root = validateInput(state, left_idx, right_idx);

		auto new_root = detail::SegmentTree::rebuildWithRange(
			root,
			left_idx,
			right_idx,
			RangeBuilder<ID>{
				.in_range     = [](ID id, usize) { return id; },
				.out_of_range = [](ID, usize) { return detail::SegmentTree::EMPTY; },
			}
		);

		return toState(new_root);
	}

	MemoryStateID Memory::eraseRange(MemoryStateID state, usize left_idx, usize right_idx) {
		auto root = validateInput(state, left_idx, right_idx);

		auto new_root = detail::SegmentTree::rebuildWithRange(
			root,
			left_idx,
			right_idx,
			RangeBuilder<ID>{
				.in_range     = [](ID, usize) { return detail::SegmentTree::EMPTY; },
				.out_of_range = [](ID id, usize) { return id; },
			}
		);

		return toState(new_root);
	}

	base::Optional<usize> Memory::access(MemoryStateID state, usize idx) const {
		auto root = validateInput(state, idx);
		auto leaf = getPathTo(root, idx).trace.at(0);

		if (leaf)
			return getValue(leaf);
		else
			return std::nullopt;
	}

	MemoryStateID Memory::setMultiple(MemoryStateID state, std::deque<std::pair<usize, usize>> vals) {
		std::ranges::sort(vals);
		using namespace std::views;
		auto idxs = vals | keys | std::ranges::to<std::deque>;

		auto root = validateInput(state, idxs);

		auto new_root = detail::SegmentTree::reconstructIdxs(
			root, idxs, [&](usize cur_idx, base::Optional<usize>) {
				CORE_ASSERT(idxs.size(), "there must be sth");
				auto [idx, val] = idxs.front();
				idxs.pop_front();
				return nodeFromIdxVar(idx, val);
			}
		);

		return toState(new_root);
	}

	MemoryStateID Memory::eraseMultiple(MemoryStateID state, std::deque<usize> idxs) {
		std::ranges::sort(idxs);
		auto root = validateInput(state, idxs);

		auto new_root = detail::SegmentTree::reconstructIdxs(
			root, idxs, [&](usize, base::Optional<usize>) { return detail::SegmentTree::EMPTY; }
		);

		return toState(new_root);
	}

	MemoryStateID Memory::erase(MemoryStateID state, usize idx) {
		return eraseMultiple(state, { idx });
	}

	MemoryStateID Memory::set(MemoryStateID state, usize idx, usize val) {
		return setMultiple(state, { { idx, val } });
	}

	usize Memory::size(MemoryStateID state) const {
		auto root = fromState(state);
		return getSize(root);
	}

	bool Memory::active(MemoryStateID state, usize idx) const {
		auto root = validateInput(state, idx);
		auto leaf = getPathTo(root, idx).trace.at(0);

		return leaf != detail::SegmentTree::EMPTY;
	}

	Memory::Memory() = default;

	MemoryIterator& MemoryIterator::operator++() {
		if_opt_some(maybe_path, path) {
			auto success = path.moveToValid(detail::SegmentTree::Dir::Right);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	MemoryIterator MemoryIterator::operator++(int) {
		auto copy = *this;
		++(*this);
		return copy;
	}

	MemoryIterator& MemoryIterator::operator--() {
		if_opt_some(maybe_path, path) {
			auto success = path.moveToValid(detail::SegmentTree::Dir::Left);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	MemoryIterator MemoryIterator::operator--(int) {
		auto copy = *this;
		--(*this);
		return copy;
	}

	MemoryIterator::MemoryIterator(const Memory& mem, MemoryStateID state, usize idx) {
		maybe_path = mem.getPathTo(Memory::fromState(state), idx);
		if (maybe_path.value().trace.at(0) == detail::SegmentTree::EMPTY)
			maybe_path = std::nullopt;
	}

	MemoryStateView::MemoryStateView(const Memory& mem, MemoryStateID id): id{ id }, mem{ mem } {}

	[[nodiscard]]
	base::Optional<usize> MemoryStateView::atMaybe(usize idx) const {
		return mem.access(id, idx);
	}

	[[nodiscard]]
	usize MemoryStateView::size() const {
		return mem.size(id);
	}

	[[nodiscard]]
	std::vector<std::pair<usize, usize>> MemoryStateView::toVec() const {
		return mem.toVec(id);
	}

	[[nodiscard]]
	auto MemoryStateView::diff(const MemoryStateView& oth) const {
		return mem.getDiff(id, oth.id);
	}

	usize MemoryStateView::operator[](usize idx) const { return *atMaybe(idx); }

	[[nodiscard]]
	bool MemoryStateView::contains(usize idx) const {
		return atMaybe(idx).has_value();
	}
}
