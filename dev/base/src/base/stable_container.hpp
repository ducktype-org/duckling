/**
 * @file stable_container.hpp
 * @brief Provides a simple expandable container with stable references.
 */
#pragma once

#include "ints.hpp"
#include "optional.hpp"
#include "ref.hpp"
#include "box.hpp"
#include <vector>

namespace base {

	/**
	 * @brief Expandable list with stable references (References are valid after the addition of new
	 * elements).
	 *
	 * @tparam Key must be convertible to and from usize.
	 *
	 * @note Add stable range based iteration (Probably with indexes)
	 */
	template<typename Data, typename Key = usize>
	requires std::constructible_from<Key, usize> && std::constructible_from<usize, Key>
	class StableVector {
		std::vector<Box<Data>> data;

	public:

		using RefT  = Ref<Data>;
		using CRefT = CRef<Data>;

		StableVector()               = default;
		StableVector(StableVector&&) = default;

		/**
		 * @note explicit delete here causes much better compiler errors.
		 * @note It is deleted because data member can't be copied in a simple way.
		 */
		StableVector(const StableVector&) = delete;

		[[nodiscard]]
		constexpr usize size() const noexcept {
			return data.size();
		}

		[[nodiscard]]
		constexpr usize empty() const noexcept {
			return data.empty();
		}

		[[nodiscard]]
		constexpr usize notEmpty() const noexcept {
			return !data.empty();
		}

		constexpr Data& operator[](Key pos) { return *data.at(static_cast<usize>(pos)); }

		constexpr const Data& operator[](Key pos) const {
			return *data.at(static_cast<usize>(pos));
		}

		Optional<RefT> getRef(Key pos) noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].refMut();
		}

		Optional<CRefT> getCRef(Key pos) const noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].ref();
		}

		constexpr Key pushBack(const Data& value) {
			auto new_ptr = box<Data>(value);
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		Key pushBack(Data&& value) {
			auto new_ptr = box<Data>(std::move(value));
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		RefT last() { return data.back().refMut(); }

		CRefT last() const { return data.back().ref(); }

		template<class... Args>
		constexpr Key emplaceBack(Args&&... args) {
			return pushBack(Data(std::forward<Args>(args)...));
		}
	};


}
