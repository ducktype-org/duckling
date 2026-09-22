#pragma once

#include <base/collections/maps.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <deque>
#include <ranges>

namespace vm {
	namespace internal {

		/**
		 * @class BaseObjIdNameMap
		 * @tparam ContainerT a container type to store types in
		 * @details We want to have two similar structures, a stable non-copyable map and an
		 * unstable copyable map. To remove duplication we define a base for both.
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
				auto index = static_cast<usize>(id);
				if (index < id_to_name.size())
					return id_to_name[index];
				else
					return {};
			}

			[[nodiscard]] constexpr base::Optional<ObjID> idOf(base::StrID name) const {
				// Mapping to copy
				return name_to_id.atMaybe(name).map([](CRef<ObjID> t) { return *t; });
			}

			[[nodiscard]]
			constexpr bool contains(ObjID id) const {
				return static_cast<usize>(id) < id_to_name.size();
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

			constexpr T& operator[](ObjID id) { return values[static_cast<usize>(id)]; }

			constexpr const T& operator[](ObjID id) const { return values[static_cast<usize>(id)]; }

			/**
			 * @brief Returns ids of inserted elements.
			 */
			constexpr auto ids() const { return std::views::iota(size_t(0), id_to_name.size()); }

			std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> allData() const {
				std::vector<std::tuple<CRef<T>, ObjID, base::StrID>> data;
				data.reserve(size());
				for (usize id = 0; id < size(); id++) {
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
			constexpr void createLink(ObjID id, base::StrID name) {
				id_to_name.push_back(name);
				name_to_id.put(name, id);
			}

			ContainerT                        values{};
			std::vector<base::StrID>          id_to_name{};
			base::HashMap<base::StrID, ObjID> name_to_id{};
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
			Base::at, Base::nameOf, Base::idOf, Base::operator[], Base::ids, Base::allData;
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
			Base::clear;
	};
}
