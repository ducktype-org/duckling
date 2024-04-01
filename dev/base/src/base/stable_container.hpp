#pragma once

#include "ints.hpp"
#include "smart_pointers.hpp"
#include "optional.hpp"
#include <concepts>
#include <type_traits>
#include <vector>

namespace base {

	template<typename Data>
	using StableListRef = base::borrow_ptr<Data>;

	template<typename Data>
	using StableListCRef = base::c_borrow_ptr<Data>;

	/**
	 * @brief Expandable list with stable references (References are valid after the addition of new elements).
	 * 
	 * @tparam Key must be convertible to and from usize.
	 * 
	 * @note Add stable range based iteration (Probably with indexes)
	 */
	template<typename Key, typename Data> 
	requires std::constructible_from<Key, usize> && std::constructible_from<usize, Key>
	class StableList {
		std::vector<base::unique_ptr<Data>> data;

	public:
		using Ref  = StableListRef<Data>;
		using CRef = StableListCRef<Data>;

		[[nodiscard]] 
		constexpr usize size() const noexcept { return data.size(); }

		[[nodiscard]] 
		constexpr usize empty() const noexcept { return data.empty(); }

		[[nodiscard]] 
		constexpr usize notEmpty() const noexcept { return !data.empty(); }

		constexpr Data& operator[](Key pos) { return *data.at(usize(pos)); }

		constexpr const Data& operator[](Key pos) const { return *data.at(usize(pos)); }

		Optional<Ref> getRef(Key pos) noexcept {
			if (usize(pos) >= size()) return {};
			return data[usize(pos)].borrow_mut();
		}

		Optional<CRef> getCRef(Key pos) const noexcept {
			if (usize(pos) >= size()) return {};
			return data[usize(pos)].borrow();
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
	using StableIntList = StableList<usize, T>;

}
