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
#include <optional>
#include <utility>
#include <vector>

namespace vm::persistent {

	std::vector<std::pair<usize, usize>> Memory::toVec(MemoryStateID state) const {
		auto root = validateInput(state, true);

		if (root == detail::SegmentTree::EMPTY) return {};

		std::vector<std::pair<usize, usize>> ans = {};

		auto [left_idx, _] = inner.getRange(root);
		auto path          = getPathTo(state, left_idx);

		do {
			auto idx = path.idx;
			auto val = *path.getValue();
			ans.emplace_back(idx, val);
		} while (path.moveToValid(Dir::Right, 0));

		return ans;
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
		return getPathTo(state, idx).getValue();
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
		return getPathTo(state, idx).pointsToValid();
	}

	Memory::Memory() = default;

	using memIt = MemoryStateView::MemoryIterator;

	memIt& memIt::operator++() {
		if_opt_some(maybe_path, path) {
			auto success = path.moveToValid(detail::SegmentTree::Dir::Right);
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
		if (maybe_path->pointsToValid()) return;
		if (maybe_path->moveToValid(detail::SegmentTree::Dir::Right) == false)
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

	usize MemoryStateView::operator[](usize idx) const { return *atMaybe(idx); }

	[[nodiscard]]
	bool MemoryStateView::contains(usize idx) const {
		return atMaybe(idx).has_value();
	}
}
