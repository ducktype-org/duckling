#pragma once

#include "base/maps.hpp"
#include "base/ref.hpp"
#include "base/string_id.hpp"
#include <base/ints.hpp>
#include <deque>
#include <ranges>
#include <type_traits>

namespace base {
	/**
	 * @note Access with ID is O(1)
	 */
	template<class T, class TID = usize>
	requires std::is_constructible_v<usize, TID> && std::is_constructible_v<TID, usize>
	class StableTypeIdNameMap {
	public:
		StableTypeIdNameMap()                                      = default;
		StableTypeIdNameMap(const StableTypeIdNameMap&)            = default;
		StableTypeIdNameMap(StableTypeIdNameMap&&)                 = default;
		StableTypeIdNameMap& operator=(const StableTypeIdNameMap&) = default;
		StableTypeIdNameMap& operator=(StableTypeIdNameMap&&)      = default;

		TID insert(T&& new_value, base::StrID name) {
			auto id = TID(values.size());
			values.push_back(std::move(new_value));
			createLink(id, name);
			return id;
		}

		TID insert(const T& new_value, base::StrID name) {
			auto id = TID(values.size());
			values.push_back(new_value);
			createLink(id, name);
			return id;
		}

		[[nodiscard]]
		base::Optional<CRef<T>> atMaybe(TID id) const {
			if (usize(id) >= values.size()) return {};
			return &values[usize(id)];
		}

		base::Optional<Ref<T>> atMaybe(TID id) {
			if (usize(id) >= values.size()) return {};
			return &values[usize(id)];
		}

		[[nodiscard]]
		base::Optional<CRef<T>> atMaybe(base::StrID name) const {
			return name_to_id.atMaybe(name).map([&](TID id) { return at(id); });
		}

		base::Optional<Ref<T>> atMaybe(base::StrID name) {
			return name_to_id.atMaybe(name).map([&](TID id) { return at(id); });
		}

		[[nodiscard]]
		CRef<T> at(TID id) const {
			return atMaybe(id).expect("Value of given ID is not stored");
		}

		Ref<T> at(TID id) { return atMaybe(id).expect("Value of given ID is not stored"); }

		[[nodiscard]]
		CRef<T> at(base::StrID name) const {
			return atMaybe(name).expect("Value of given name is not stored");
		}

		Ref<T> at(base::StrID name) {
			return atMaybe(name).expect("Value of given name is not stored");
		}

		base::Optional<TID> nameOf(TID id) const { return id_to_name.atMaybe(id); }

		[[nodiscard]] base::Optional<base::StrID> idOf(base::StrID name) const {
			return name_to_id.atMaybe(name);
		}

		[[nodiscard]]
		bool contains(TID id) const {
			return id_to_name.contains(id);
		}

		bool contains(TID id) { return id_to_name.contains(id); }

		[[nodiscard]] bool contains(base::StrID name) const { return name_to_id.contains(name); }

		bool contains(base::StrID name) { return name_to_id.contains(name); }

		[[nodiscard]]
		auto begin() const {
			return values.begin();
		}

		auto begin() { return values.begin(); }

		[[nodiscard]]
		auto end() const {
			return values.end();
		}

		auto end() { return values.end(); }

		[[nodiscard]] usize size() const { return values.size(); }

		auto ids() { return id_to_name | std::views::values; }

		auto ids() const { return id_to_name | std::views::values; }

		auto names() { return name_to_id | std::views::values; }

		auto names() const { return name_to_id | std::views::values; }

		T& operator[](TID id) { return values[static_cast<usize>(id)]; }

		const T& operator[](TID id) const { return values[static_cast<usize>(id)]; }

	private:
		void createLink(TID id, base::StrID name) {
			id_to_name.put(id, name);
			name_to_id.put(name, id);
		}

		/**
		 * @note We are using std::deque here, because references to its data are always valid (do
		 * not become dangling).
		 * This is because:
		 * * Deleting from the structure is not possible.
		 * * deque does not relocate memory
		 */
		std::deque<T>                   values{};
		base::HashMap<TID, base::StrID> id_to_name{};
		base::HashMap<base::StrID, TID> name_to_id{};
	};


}
