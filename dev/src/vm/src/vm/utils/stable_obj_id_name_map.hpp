#pragma once

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <deque>
#include <ranges>

namespace vm {
	namespace internal {
		template<class T, class ContainerT, class ObjID = u64>
		requires(std::constructible_from<ObjID, usize> && std::constructible_from<usize, ObjID>)
		class BaseObjIdNameMap {
		public:
			BaseObjIdNameMap()                                     = default;
			BaseObjIdNameMap(BaseObjIdNameMap&&)                   = default;
			BaseObjIdNameMap(const BaseObjIdNameMap&)              = default;
			BaseObjIdNameMap& operator=(BaseObjIdNameMap&&) &      = default;
			BaseObjIdNameMap& operator=(const BaseObjIdNameMap&) & = default;

			constexpr ObjID insert(T&& new_value, base::StrID name) {
				auto id = ObjID(values.size());
				values.push_back(std::move(new_value));
				createLink(id, name);
				return id;
			}

			constexpr ObjID insert(const T& new_value, base::StrID name) {
				auto id = ObjID(values.size());
				values.push_back(new_value);
				createLink(id, name);
				return id;
			}

			[[nodiscard]]
			constexpr base::Optional<CRef<T>> atMaybe(ObjID id) const {
				if (usize(id) >= values.size()) return {};
				return &values[usize(id)];
			}

			constexpr base::Optional<Ref<T>> atMaybe(ObjID id) {
				if (usize(id) >= values.size()) return {};
				return &values[usize(id)];
			}

			[[nodiscard]]
			constexpr base::Optional<CRef<T>> atMaybe(base::StrID name) const {
				return name_to_id.atMaybe(name).map([&](CRef<ObjID> id) { return at(*id); });
			}

			constexpr base::Optional<Ref<T>> atMaybe(base::StrID name) {
				return name_to_id.atMaybe(name).map([&](Ref<ObjID> id) { return at(*id); });
			}

			[[nodiscard]]
			constexpr CRef<T> at(ObjID id) const {
				return atMaybe(id).expect("Value of given ID is not stored");
			}

			constexpr Ref<T> at(ObjID id) {
				return atMaybe(id).expect("Value of given ID is not stored");
			}

			[[nodiscard]]
			constexpr CRef<T> at(base::StrID name) const {
				return atMaybe(name).expect("Value of given name is not stored");
			}

			constexpr Ref<T> at(base::StrID name) {
				return atMaybe(name).expect("Value of given name is not stored");
			}

			constexpr base::Optional<base::StrID> nameOf(ObjID id) const {
				return id_to_name.atMaybe(id).map([](auto ref) {
					return base::StrID(std::move(*ref));
				});
			}

			[[nodiscard]] constexpr base::Optional<ObjID> idOf(base::StrID name) const {
				// Mapping to copy
				return name_to_id.atMaybe(name).map([](CRef<ObjID> t) { return *t; });
			}

			[[nodiscard]]
			constexpr bool contains(ObjID id) const {
				return id_to_name.contains(id);
			}

			constexpr bool contains(ObjID id) { return id_to_name.contains(id); }

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

			constexpr T& operator[](ObjID id) { return values[static_cast<usize>(id)]; }

			constexpr const T& operator[](ObjID id) const { return values[static_cast<usize>(id)]; }

			/**
			 * @brief Returns ids of inserted elements.
			 */
			constexpr auto ids() const { return id_to_name | std::views::keys; }

			std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> allData() const {
				std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> data;
				for (usize id = 0; id < size(); id++) {
					ObjID tid = ObjID(id);
					data.emplace_back(at(tid), tid, *nameOf(tid));
				}
				return data;
			}

		private:
			constexpr void createLink(ObjID id, base::StrID name) {
				id_to_name.put(id, name);
				name_to_id.put(name, id);
			}

			/**
			 * @note We are using std::deque here, because references to its data are always valid
			 * (do not become dangling). This is because:
			 * * Deleting from the structure is not possible.
			 * * deque does not relocate memory (unlike vector)
			 */
			ContainerT                        values{};
			base::HashMap<ObjID, base::StrID> id_to_name{};
			base::HashMap<base::StrID, ObjID> name_to_id{};
		};
	}

	/**
	 * @brief A Stable container, that maps an element of type T with a name, and
	 * assigns an ID to it. It is used by loader to map functions and type to ids.
	 * If T is copyable, then this structure is as well.
	 * After copy, new elements inserted to it will be given new, consecutive IDs, so
	 * previously stored IDs will map to equal, but copied objects. (old references will not break).
	 * @note It's not possible to erase values from this structure.
	 */
	template<class T, class ObjID = u64>
	requires(std::constructible_from<ObjID, usize> && std::constructible_from<usize, ObjID>)
	class StableObjIdNameMap: private internal::BaseObjIdNameMap<T, std::deque<T>, ObjID> {
		using Base = internal::BaseObjIdNameMap<T, std::deque<T>, ObjID>;

	public:
		StableObjIdNameMap()                                       = default;
		StableObjIdNameMap(StableObjIdNameMap&&)                   = default;
		StableObjIdNameMap& operator=(StableObjIdNameMap&&) &      = default;
		StableObjIdNameMap(const StableObjIdNameMap&)              = default;
		StableObjIdNameMap& operator=(const StableObjIdNameMap&) & = default;

		// The copy does not have the same pointers, it is not stable.
		// Thus copying is more often then not a programming error.
		// StableObjIdNameMap(const StableObjIdNameMap&)            = delete;
		// StableObjIdNameMap& operator=(const StableObjIdNameMap&) = delete;

		using Base::contains, Base::begin, Base::end, Base::size, Base::insert, Base::atMaybe,
			Base::at, Base::nameOf, Base::idOf, Base::operator[], Base::ids, Base::allData;
	};

	template<class T, class ObjID = u64>
	requires(std::constructible_from<ObjID, usize> && std::constructible_from<usize, ObjID>)
	class ObjIdNameMap: private internal::BaseObjIdNameMap<T, std::vector<T>, ObjID> {
		using Base = internal::BaseObjIdNameMap<T, std::vector<T>, ObjID>;

	public:
		ObjIdNameMap()                                 = default;
		ObjIdNameMap(ObjIdNameMap&&)                   = default;
		ObjIdNameMap(const ObjIdNameMap&)              = default;
		ObjIdNameMap& operator=(ObjIdNameMap&&) &      = default;
		ObjIdNameMap& operator=(const ObjIdNameMap&) & = default;

		using Base::contains, Base::begin, Base::end, Base::size, Base::insert, Base::atMaybe,
			Base::at, Base::nameOf, Base::idOf, Base::operator[], Base::ids, Base::allData;
	};
}
