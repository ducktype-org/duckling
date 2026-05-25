#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace vm::persistent {

	template<typename Key, typename Val, typename Hasher = std::hash<Key>>
	class DummyHashMap {
		std::vector<base::HashMap<Key, Val, Hasher>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		[[nodiscard]]
		usize erase(usize state, const std::vector<Key>& removed_keys) {
			auto copy = validateState(state);

			for (auto removed_key: removed_keys) {
				if (!copy.contains(removed_key))
					throw std::invalid_argument("Trying to remove a non present key");

				copy.erase(removed_key);
			}

			copies.emplace_back(copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		usize insert(usize state, const Key& k, const Val& v) {
			auto copy = validateState(state);

			if (copy.contains(k)) throw std::invalid_argument("overriding a present value");
			copy.put(k, v);

			copies.emplace_back(copy);

			return copies.size() - 1;
		}

		[[nodiscard]]
		const Val& at(usize state, const Key& k) const {
			return validateState(state).at(k);
		}

		[[nodiscard]]
		bool contains(usize state, const Key& k) const {
			return validateState(state).contains(k);
		}

		[[nodiscard]]
		usize size(usize state) const {
			return validateState(state).size();
		}

		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1) == validateState(state_2);
		}

		DummyHashMap() { copies.emplace_back(base::HashMap<Key, Val, Hasher>{}); }
	};
}
