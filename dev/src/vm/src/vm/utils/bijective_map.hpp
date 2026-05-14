#pragma once

#include <base/collections/maps.hpp>

#include <functional>
#include <stdexcept>

namespace vm::persistent::detail {
	/**
	 * @brief Helper structure to contain binding between values of type L and R. For pair (l1, r1)
	 * there doesn't exist any pair (l2, r2) s.t. `l1 == l2` or `r1 == r2`
	 *
	 * @tparam L type of bound values
	 * @tparam R type of bound values
	 * @tparam HashL hash object used for type L
	 * @tparam HashR hash object used for type R
	 */
	template<typename L, typename R, typename HashL = std::hash<L>, typename HashR = std::hash<R>>
	class BijectiveMap {
		base::HashMap<L, R, HashL> left_right;
		base::HashMap<R, L, HashR> right_left;

	public:
		base::Optional<R> atLeftOpt(const L& left) const { return left_right.atMaybeCopy(left); }

		base::Optional<L> atRightOpt(const R& rght) const { return right_left.atMaybeCopy(rght); }

		const R& atLeft(const L& left) const { return left_right.at(left); }

		const L& atRight(const R& rght) const { return right_left.at(rght); }

		[[nodiscard]]
		size_t size() const {
			CORE_ASSERT(
				left_right.size() == right_left.size(),
				"bijection requires that both sets are equally big"
			);
			return left_right.size();
		}

		std::pair<bool, R> emplaceByLeft(const L& left, const R& rght) {
			if (auto it = left_right.find(left); it != left_right.end())
				return { false, it->second };

			if (right_left.find(rght) != right_left.end())
				throw std::invalid_argument("right element is already bound");

			right_left.put(rght, left);
			left_right.put(left, rght);

			return { true, left_right[left] };
		}

		std::pair<bool, L> emplaceByRight(const L& left, const R& rght) {
			if (auto it = right_left.find(rght); it != right_left.end())
				return { false, it->second };

			if (left_right.find(left) != left_right.end())
				throw std::invalid_argument("left element is already bound");

			left_right.put(left, rght);
			right_left.put(rght, left);

			return { true, right_left[rght] };
		}

		auto leftToRight() const { return left_right; }

		auto rightToLeft() const { return right_left; }

		void clear() {
			left_right.clear();
			right_left.clear();
		}

		[[nodiscard]]
		bool empty() const {
			return (size() == 0);
		}

		auto begin() const { return left_right.begin(); }

		auto end() const { return left_right.end(); }
	};
}
