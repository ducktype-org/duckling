#include "memory.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/tree.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace vm::persistent {

	std::vector<std::pair<usize, usize>> Memory::toVec(MemoryStateID state) const {
		auto root = validateInput(state, true);

		if (root == detail::SegmentTree::EMPTY) return {};

		std::vector<std::pair<usize, usize>> ans = {};

		auto [left_idx, _] = inner.getRange(root);
		auto path          = *getPathTo(state, left_idx);

		do {
			auto idx = path.getIdx();
			auto val = path.getValue();
			ans.emplace_back(idx, val);
		} while (path.moveToValid(Dir::Rght, 0));

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

		inner.rebuildFromTwo(
			root_1,
			root_2,
			detail::SegmentTree::MergeBuilder<void>{
				.only_1   = add_left,
				.only_2   = add_right,
				.the_same = [](ID, usize) {},
				.conflicts
				= [&](usize idx, usize val_1, usize val_2) { ans.emplace_back(idx, val_1, val_2); },
			}
		);

		return ans;
	}

	MemoryStateID Memory::merge(MemoryStateID state_1, MemoryStateID state_2, ConflictPolicy policy) {
		auto [root_1, root_2] = validateInput(state_1, state_2);

		std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>> ans = {};

		auto new_root = inner.rebuildFromTwo(
			root_1,
			root_2,
			detail::SegmentTree::MergeBuilder<ID>{
				.only_1    = [](ID id, usize) { return id; },
				.only_2    = [](ID id, usize) { return id; },
				.the_same  = [](ID id, usize) { return id; },
				.conflicts = [&](usize idx, usize val_1, usize val_2) -> ID {
					match_optional(policy(idx, val_1, val_2)) {
						opt_some(val) { return inner.emplaceLeaf(idx, val); }
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

		auto new_root = inner.rebuildRange(
			root,
			left_idx,
			right_idx,
			detail::SegmentTree::RangeBuilder<ID>{
				.in_range     = [](ID id, usize) { return id; },
				.out_of_range = [](ID, usize) { return detail::SegmentTree::EMPTY; },
			}
		);

		return toState(new_root);
	}

	MemoryStateID Memory::eraseRange(MemoryStateID state, usize left_idx, usize right_idx) {
		auto root = validateInput(state, left_idx, right_idx);

		auto new_root = inner.rebuildRange(
			root,
			left_idx,
			right_idx,
			detail::SegmentTree::RangeBuilder<ID>{
				.in_range     = [](ID, usize) { return detail::SegmentTree::EMPTY; },
				.out_of_range = [](ID id, usize) { return id; },
			}
		);

		return toState(new_root);
	}

	base::Optional<usize> Memory::access(MemoryStateID state, usize idx) const {
		validateInput(state, idx);

		if_opt_some(getPathTo(state, idx), path) { return path.getValue(); }
		return std::nullopt;
	}

	MemoryStateID Memory::setMultiple(MemoryStateID state, std::deque<std::pair<usize, usize>> vals) {
		std::ranges::sort(vals);
		using namespace std::views;
		std::deque<usize> idxs{};
		for (auto [idx, _]: vals) idxs.emplace_back(idx);

		auto root = validateInput(state, idxs);

		auto new_root
			= inner.reconstructLeaves(root, idxs, [&](usize cur_idx, base::Optional<usize>) -> ID {
				  CORE_ASSERT(vals.size(), "there must be sth");
				  auto [idx, val] = vals.front();
				  vals.pop_front();
				  CORE_ASSERT(cur_idx == idx, "expected other idx");
				  return inner.emplaceLeaf(idx, val);
			  });

		return toState(new_root);
	}

	MemoryStateID Memory::eraseMultiple(MemoryStateID state, std::deque<usize> idxs) {
		std::ranges::sort(idxs);
		auto root = validateInput(state, idxs);

		auto new_root = inner.reconstructLeaves(root, idxs, [&](usize, base::Optional<usize>) {
			return detail::SegmentTree::EMPTY;
		});

		return toState(new_root);
	}

	MemoryStateID Memory::erase(MemoryStateID state, usize idx) {
		return eraseMultiple(state, { idx });
	}

	MemoryStateID Memory::set(MemoryStateID state, usize idx, usize val) {
		return setMultiple(state, { { idx, val } });
	}

	usize Memory::size(MemoryStateID state) const {
		auto root = validateInput(state);
		return inner.getSize(root);
	}

	bool Memory::active(MemoryStateID state, usize idx) const {
		return getPathTo(state, idx).has_value();
	}

	Memory::Memory() = default;

	using memIt = MemoryStateView::MemoryIterator;

	memIt& memIt::operator++() {
		if_opt_some(maybe_path, path) {
			auto success = path.moveToValid(detail::SegmentTree::Dir::Rght);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	memIt memIt::operator++(int) {
		auto copy = *this;
		++(*this);
		return copy;
	}

	memIt& memIt::operator--() {
		if_opt_some(maybe_path, path) {
			auto success = path.moveToValid(detail::SegmentTree::Dir::Left);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	memIt memIt::operator--(int) {
		auto copy = *this;
		--(*this);
		return copy;
	}

	memIt::MemoryIterator(const Memory& mem, MemoryStateID state, usize idx) {
		maybe_path = mem.getPathTo(state, idx);
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
