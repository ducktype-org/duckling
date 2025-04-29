#pragma once

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <deque>
#include <ranges>

namespace vm {
	/**
	 * @brief A Stable container, that maps an element of type T with a name, and
	 * assigns an ID to it. It is used by loader to map functions and type to ids.
	 * If T is copyable, then this structure is as well.
	 * After copy, new elements inserted to it will be given new, consecutive IDs, so
	 * previously stored IDs will map to equal, but copied objects. (old references will not break).
	 * @note It's not possible to erase values from this structure.
	 */
	template<class T, class TID = u64>
	requires(std::constructible_from<TID, usize> && std::constructible_from<usize, TID>)
	class StableTypeIdNameMap {
	public:
		StableTypeIdNameMap()                                      = default;
		StableTypeIdNameMap(const StableTypeIdNameMap&)            = default;
		StableTypeIdNameMap(StableTypeIdNameMap&&)                 = default;
		StableTypeIdNameMap& operator=(const StableTypeIdNameMap&) = default;
		StableTypeIdNameMap& operator=(StableTypeIdNameMap&&)      = default;

		constexpr TID insert(T&& new_value, base::StrID name) {
			auto id = TID(values.size());
			values.push_back(std::move(new_value));
			createLink(id, name);
			return id;
		}

		constexpr TID insert(const T& new_value, base::StrID name) {
			auto id = TID(values.size());
			values.push_back(new_value);
			createLink(id, name);
			return id;
		}

		[[nodiscard]]
		constexpr base::Optional<CRef<T>> atMaybe(TID id) const {
			if (usize(id) >= values.size()) return {};
			return &values[usize(id)];
		}

		constexpr base::Optional<Ref<T>> atMaybe(TID id) {
			if (usize(id) >= values.size()) return {};
			return &values[usize(id)];
		}

		[[nodiscard]]
		constexpr base::Optional<CRef<T>> atMaybe(base::StrID name) const {
			return name_to_id.atMaybe(name).map([&](TID id) { return at(id); });
		}

		constexpr base::Optional<Ref<T>> atMaybe(base::StrID name) {
			return name_to_id.atMaybe(name).map([&](TID id) { return at(id); });
		}

		[[nodiscard]]
		constexpr CRef<T> at(TID id) const {
			return atMaybe(id).expect("Value of given ID is not stored");
		}

		constexpr Ref<T> at(TID id) {
			return atMaybe(id).expect("Value of given ID is not stored");
		}

		[[nodiscard]]
		constexpr CRef<T> at(base::StrID name) const {
			return atMaybe(name).expect("Value of given name is not stored");
		}

		constexpr Ref<T> at(base::StrID name) {
			return atMaybe(name).expect("Value of given name is not stored");
		}

		constexpr base::Optional<base::StrID> nameOf(TID id) const {
			return id_to_name.atMaybe(id).map([](auto&& ref) { return base::StrID(ref); });
		}

		[[nodiscard]] constexpr base::Optional<TID> idOf(base::StrID name) const {
			// Mapping to copy
			return name_to_id.atMaybe(name).map([](TID t) { return t; });
		}

		[[nodiscard]]
		constexpr bool contains(TID id) const {
			return id_to_name.contains(id);
		}

		constexpr bool contains(TID id) { return id_to_name.contains(id); }

		[[nodiscard]] constexpr bool contains(base::StrID name) const {
			return name_to_id.contains(name);
		}

		constexpr bool contains(base::StrID name) { return name_to_id.contains(name); }

		[[nodiscard]]
		constexpr auto begin() const {
			return values.begin();
		}

		constexpr auto begin() { return values.begin(); }

		[[nodiscard]]
		constexpr auto end() const {
			return values.end();
		}

		constexpr auto end() { return values.end(); }

		[[nodiscard]] constexpr usize size() const { return values.size(); }

		constexpr T& operator[](TID id) { return values[static_cast<usize>(id)]; }

		constexpr const T& operator[](TID id) const { return values[static_cast<usize>(id)]; }

		constexpr auto ids() const { return id_to_name | std::views::keys; }

		std::vector<std::tuple<CRef<T>, TID, base::StrID>> allData() const {
			std::vector<std::tuple<CRef<T>, TID, base::StrID>> data;
			for (usize id = 0; id < size(); id++) {
				TID tid = TID(id);
				data.emplace_back(at(tid), tid, *nameOf(tid));
			}
			return data;
		}

	private:
		constexpr void createLink(TID id, base::StrID name) {
			id_to_name.put(id, name);
			name_to_id.put(name, id);
		}

		/**
		 * @note We are using std::deque here, because references to its data are always valid (do
		 * not become dangling).
		 * This is because:
		 * * Deleting from the structure is not possible.
		 * * deque does not relocate memory (unlike vector)
		 */
		std::deque<T>                   values{};
		base::HashMap<TID, base::StrID> id_to_name{};
		base::HashMap<base::StrID, TID> name_to_id{};
	};
}
