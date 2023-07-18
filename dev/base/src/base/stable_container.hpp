#pragma once

#include <vector>
#include "smart_pointers.hpp"
#include "option.hpp"
#include <concepts>

namespace base {
	
	template<typename Data>
	using StableListRef = base::borrow_ptr<Data>;

	template<typename Data>
	using StableListCRef = base::c_borrow_ptr<Data>;

	/**
	 * @brief Key must be „standard” numeric value such as:
	 * integer
	 * strongly typed int
	 * NamedID
	 * 
	 * Right now StableList keys must be convertible to and from size_t
	 * 
	 * @TODO make concept to check it
	 * @TODO add range based iteration
	 */
	template<typename Key, typename Data> 
	class StableList {
		/**
		 * @brief @TODO
		 * for now it is simple, naive implementation 
		 * in the future change it to something better
		 */

		std::vector<base::unique_ptr<Data>> data;

	public:
		using Ref = StableListRef<Data>;
		using CRef = StableListCRef<Data>;

		constexpr std::size_t size() const noexcept { return data.size(); }
		constexpr std::size_t empty() const noexcept { return data.empty(); }
		constexpr std::size_t notEmpty() const noexcept { return !data.empty(); }
		
		/**
		 * @brief Quick, unsafe, constexpr access 
		 */
		constexpr Data& operator[](Key pos) {
			return *data.at(std::size_t(pos));
		};
		/**
		 * @brief Quick, unsafe, constexpr access 
		 */
		constexpr const Data& operator[](Key pos) const {
			return *data.at(std::size_t(pos));
		};

		option<Ref> getRef(Key pos) noexcept {
			if (size_t(pos) >= size()) return none<Ref>();
			return data[size_t(pos)].borrow_mut();
		}

		option<CRef> getCRef(Key pos) const noexcept {
			if (size_t(pos) >= size()) return none<CRef>();
			return data[size_t(pos)].borrow();
		}

		constexpr Key pushBack(const Data& value) {
			auto new_ptr = base::make_unique<Data>(value);
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		Key pushBack(Data&& value) {
			auto new_ptr = base::make_unique<Data>(std::move(value));
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		Ref last() { return data.back().borrow_mut(); }
		CRef last() const { return data.back().borrow(); }

		template<class... Args>
		constexpr Key emplaceBack(Args&&... args) {
			return pushBack(Data(std::forward<Args...>(args...)));
		}
	};

	template<typename T>
	using StableIntList = StableList<std::size_t, T>;

}