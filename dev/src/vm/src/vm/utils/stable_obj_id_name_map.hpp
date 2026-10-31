// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <deque>
#include <ranges>

namespace vm {
	/**
	 * @brief The ID of the first element inserted to an ID name map, so the zero ID never refers to
	 * an element.
	 */
	inline constexpr usize FIRST_OBJ_ID = 1;

	namespace internal {

		/**
		 * @class BaseObjIdNameMap
		 * @tparam ContainerT a container type to store types in
		 * @details We want to have two similar structures, a stable non-copyable map and an
		 * unstable copyable map. To remove duplication we define a base for both.
		 * IDs are consecutive and start from `FIRST_OBJ_ID`.
		 */
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
				auto id = nextID();
				values.push_back(std::move(new_value));
				createLink(id, name);
				return id;
			}

			constexpr ObjID insert(const T& new_value, base::StrID name) {
				auto id = nextID();
				values.push_back(new_value);
				createLink(id, name);
				return id;
			}

			[[nodiscard]]
			constexpr base::Optional<CRef<T>> atMaybe(ObjID id) const {
				if (!contains(id)) return {};
				return &values[indexOf(id)];
			}

			constexpr base::Optional<Ref<T>> atMaybe(ObjID id) {
				if (!contains(id)) return {};
				return &values[indexOf(id)];
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
				if (contains(id))
					return id_to_name[indexOf(id)];
				else
					return {};
			}

			[[nodiscard]] constexpr base::Optional<ObjID> idOf(base::StrID name) const {
				// Mapping to copy
				return name_to_id.atMaybe(name).map([](CRef<ObjID> t) { return *t; });
			}

			[[nodiscard]]
			constexpr bool contains(ObjID id) const {
				return static_cast<usize>(id) >= FIRST_OBJ_ID
				    && static_cast<usize>(id) - FIRST_OBJ_ID < id_to_name.size();
			}

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

			/**
			 * @brief Returns the ID the next inserted element will get.
			 */
			[[nodiscard]] constexpr ObjID nextID() const {
				return ObjID(FIRST_OBJ_ID + values.size());
			}

			constexpr T& operator[](ObjID id) { return values[indexOf(id)]; }

			constexpr const T& operator[](ObjID id) const { return values[indexOf(id)]; }

			/**
			 * @brief Returns ids of inserted elements.
			 */
			constexpr auto ids() const {
				return std::views::iota(FIRST_OBJ_ID, FIRST_OBJ_ID + id_to_name.size());
			}

			std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> allData() const {
				std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> data;
				data.reserve(size());
				for (usize id = FIRST_OBJ_ID; id < FIRST_OBJ_ID + size(); id++) {
					ObjID tid = ObjID(id);
					data.emplace_back(at(tid), tid, *nameOf(tid));
				}
				return data;
			}

			constexpr void clear() {
				values.clear();
				id_to_name.clear();
				name_to_id.clear();
			}

		private:
			static constexpr usize indexOf(ObjID id) {
				return static_cast<usize>(id) - FIRST_OBJ_ID;
			}

			constexpr void createLink(ObjID id, base::StrID name) {
				id_to_name.push_back(name);
				name_to_id.put(name, id);
			}

			ContainerT                        values{};
			std::vector<base::StrID>          id_to_name{};
			base::StableHashMap<base::StrID, ObjID> name_to_id{};
		};
	}

	/**
	 * @brief A Stable container, that maps an element of type T with a name, and
	 * assigns an ID to it.
	 * @note It's not possible to erase values from this structure.
	 */
	template<class T, class ObjID = u64>
	requires(std::constructible_from<ObjID, usize> && std::constructible_from<usize, ObjID>)
	class StableObjIdNameMap: private internal::BaseObjIdNameMap<T, std::deque<T>, ObjID> {
		using Base = internal::BaseObjIdNameMap<T, std::deque<T>, ObjID>;

	public:
		StableObjIdNameMap()                                  = default;
		StableObjIdNameMap(StableObjIdNameMap&&)              = default;
		StableObjIdNameMap& operator=(StableObjIdNameMap&&) & = default;

		// The copy does not have the same pointers, it is not stable.
		// Thus, copying is more often than not a programming error.
		StableObjIdNameMap(const StableObjIdNameMap&)            = delete;
		StableObjIdNameMap& operator=(const StableObjIdNameMap&) = delete;

		using Base::contains, Base::begin, Base::end, Base::size, Base::insert, Base::atMaybe,
			Base::at, Base::nameOf, Base::idOf, Base::operator[], Base::ids, Base::allData,
			Base::nextID;
	};

	/**
	 * @brief Container, that maps an element of type T with a name, and
	 * assigns an ID to it.
	 * If T is copyable, then this structure is as well.
	 * After copy previously stored IDs will map to equal, but copied objects.
	 * @note It's not possible to erase values from this structure.
	 */
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
			Base::at, Base::nameOf, Base::idOf, Base::operator[], Base::ids, Base::allData,
			Base::clear, Base::nextID;
	};
}
