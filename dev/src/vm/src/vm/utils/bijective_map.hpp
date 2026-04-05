#pragma once

#include <base/collections/maps.hpp>

#include <functional>
#include <optional>
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
		base::Optional<R> atLeftOpt(L left) const {
			auto it = left_right.find(left);
			return (it == left_right.end()) ? std::nullopt : it->second;
		}

		base::Optional<L> atRightOpt(R rght) const {
			auto it = right_left.find(rght);
			return (it == right_left.end()) ? std::nullopt : it->second;
		}

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

		std::pair<bool, const R&> emplaceByLeft(const L& left, const R& rght) {
			if (right_left.find(rght) != right_left.end())
				throw std::invalid_argument("right element is already bound");

			auto itl = left_right.find(left);
			if (itl != left_right.end()) return { false, itl->second };

			right_left.emplace(rght, left);
			auto [_, it] = left_right.emplace(left, rght);

			return { true, it->second };
		}

		std::pair<bool, const L&> emplaceByRight(const L& left, const R& rght) {
			if (left_right.find(left) != left_right.end())
				throw std::invalid_argument("left element is already bound");

			auto itr = right_left.find(rght);
			if (itr != right_left.end()) return { false, itr->second };

			left_right.emplace(left, rght);
			auto [_, it] = right_left.emplace(rght, left);

			return { true, it->second };
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
