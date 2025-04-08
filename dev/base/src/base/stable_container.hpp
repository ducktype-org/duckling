/**
 * @file stable_container.hpp
 * @brief Provides a simple expandable container with stable references.
 */
#pragma once

#include "box.hpp"
#include "ref.hpp"

#include <vector>

namespace base {

	/**
	 * @brief Expandable list (like std::vector), but with stable references (References are valid
	 * after the addition of new elements).
	 *
	 * @tparam Key must be convertible to and from usize.
	 *
	 * @note Add stable range based iteration (Probably with indexes)
	 */
	template<typename Data>
	class StableVector final {
		std::vector<Box<Data>> data;
		using size_type = std::vector<Box<Data>>::size_type;

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

		/**
		 * @note It is pointer-wise comparision
		 * @todo change it to value-wise comparision once StableVector refactor is introduced
		 * @note it is used to compare mir::Function, lir::Function
		 * There are a lot of questions regarding how hout, mir, lir objects
		 * should be compared and hashed.
		 */
		bool operator==(const StableVector& other) const = default;

		[[nodiscard]]
		constexpr size_type size() const noexcept {
			return data.size();
		}

		[[nodiscard]]
		constexpr bool empty() const noexcept {
			return data.empty();
		}

		[[nodiscard]]
		constexpr bool notEmpty() const noexcept {
			return not data.empty();
		}

		constexpr Ref<Data> operator[](size_type pos) { return data.at(pos).refMut(); }

		constexpr CRef<Data> operator[](size_type pos) const { return data.at(pos).ref(); }

		constexpr void pushBack(const Data& value) {
			auto new_ptr = makeBox<Data>(value);
			data.emplace_back(std::move(new_ptr));
		}

		void pushBack(Data&& value) {
			auto new_ptr = makeBox<Data>(std::move(value));
			data.emplace_back(std::move(new_ptr));
		}

		RefT last() { return data.back().refMut(); }

		CRefT last() const { return data.back().ref(); }

		/**
		 * Returns index of the last element (i.e. size - 1).
		 * Undefined if the size is 0.
		 */
		constexpr size_type lastIndex() const { return size() - 1; }

		template<class... Args>
		constexpr void emplaceBack(Args&&... args) {
			auto new_ptr = makeBox<Data>(std::forward<Args>(args)...);
			data.emplace_back(std::move(new_ptr));
		}

		auto begin() const { return data.begin(); }

		auto end() const { return data.end(); }
	};
}
