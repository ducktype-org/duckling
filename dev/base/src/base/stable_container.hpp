#pragma once

#include "ints.hpp"
#include "optional.hpp"
#include <vector>

namespace base {

	template<typename Data>
	using StableVectorRef = borrow_ptr<Data>;

	template<typename Data>
	using StableVectorCRef = c_borrow_ptr<Data>;

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
		std::vector<unique_ptr<Data>> data;

	public:
		using Ref  = StableVectorRef<Data>;
		using CRef = StableVectorCRef<Data>;

		StableVector()               = default;
		StableVector(StableVector&&) = default;

		/**
		 * @note explicit delete here causes much better compiler errors
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

		Optional<Ref> getRef(Key pos) noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].borrow_mut();
		}

		Optional<CRef> getCRef(Key pos) const noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].borrow();
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
			return pushBack(Data(std::forward<Args>(args)...));
		}
	};


}
