/**
 * @file stable_container.hpp
 * @brief Provides a simple expandable container with stable references.
 */
#pragma once

#include "box.hpp"
#include "ref.hpp"

#include <deque>
#include <iterator>

namespace base {
	namespace internal {
		/**
		* @class Secret
		* @brief purpose of this class is to protect subclass from mimischievous use
		* @note contents are marked private, and thus only friends can make use of them
		* @tparam BaseCRTP - a base type for [CRTP](https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/)
		*/
		template<typename BaseCRTP>
		class Secret {
		private:
			friend BaseCRTP;

			/**
			* @class Wrap
			* @brief this is the class from which BaseCRTP should publicly inherit random access operations (iterator boilerplate)
			* @note BaseCRTP should hold visible for Wrap member variable `it` and static method `factory(Wrapped)->BaseCRTP`
			* @tparam Wrapped - type of iterator which is being wrapped (type of `it`)
			*/
			template<typename Wrapped>
			class Wrap {
			private:
				Wrap() = default;
				friend BaseCRTP;
			public:
				using difference_type   = typename Wrapped::difference_type;
				using iterator_category = std::random_access_iterator_tag;

				BaseCRTP& operator++(this BaseCRTP& self) {
					++self.it;
					return self;
				}

				BaseCRTP operator++(this BaseCRTP &self, int) {
					return BaseCRTP::factory(self.it++);
				}

				BaseCRTP& operator--(this BaseCRTP& self) {
					--self;
					return self;
				}

				BaseCRTP operator--(this BaseCRTP &self, int) {
					return BaseCRTP::factory(self.it--);
				}

				BaseCRTP& operator+=(this BaseCRTP& self, difference_type diff) {
					self.it += diff;
					return self;
				}

				BaseCRTP operator+(this const BaseCRTP& self, const difference_type diff) {
					return BaseCRTP::factory(self.it + diff);
				}

				friend BaseCRTP operator+(const difference_type diff, const BaseCRTP& iter) {
					return BaseCRTP::factory(iter.it + diff);
				}

				BaseCRTP& operator-=(this const BaseCRTP& self, difference_type diff) {
					self.it -= diff;
					return self;
				}

				BaseCRTP operator-(this const BaseCRTP& self, const difference_type diff) {
					return BaseCRTP::factory(self.it - diff);
				}

				difference_type operator-(this const BaseCRTP& self, const BaseCRTP& other) {
					return self.it - other.it;
				}

				bool operator==(this const BaseCRTP& self, const BaseCRTP& other) {
					return self.it == other.it;
				}

				auto operator<=>(this const BaseCRTP& self, const BaseCRTP& other) {
					return self.it <=> other.it;
				}
			};
		};

		/**
		 * @class BaseStableVector
		 * @brief Wrapper class over a container (std::vector<Box>) which returns Ref/CRef on access.
		 * @details It is used to allow a seamless conversion from StableVector<Data> to
		 * StableVector<const Data>.
		 */
		template<class Data>
		class BaseStableVector {
			using ContainerT = std::vector<Box<Data>>;
			ContainerT data;
			static_assert(
				std::is_same_v<typename ContainerT::size_type, usize>,
				"When this fail, figure out what to do."
			);

			using I = typename ContainerT::iterator;
			using CI = typename ContainerT::const_iterator; 

			BaseStableVector(ContainerT&& data): data(std::move(data)) {}

			BaseStableVector(const ContainerT& data): data(data) {}

		public:
			using RefT  = Ref<Data>;
			using CRefT = CRef<Data>;

			BaseStableVector()                   = default;
			BaseStableVector(BaseStableVector&&) = default;

			/**
			 * @note explicit delete here causes much better compiler errors.
			 * @note It is deleted because data member can't be copied in a simple way.
			 */
			BaseStableVector(const BaseStableVector&) = delete;

			[[nodiscard]]
			usize size() const noexcept {
				return data.size();
			}

			[[nodiscard]]
			bool empty() const noexcept {
				return data.empty();
			}

			[[nodiscard]]
			bool notEmpty() const noexcept {
				return not data.empty();
			}

			[[nodiscard]]
			RefT operator[](usize pos) {
				return data.at(pos).refMut();
			}

			[[nodiscard]]
			CRefT operator[](usize pos) const {
				return data.at(pos).ref();
			}

			void pushBack(const Data& value) { data.emplace_back(makeBox<Data>(value)); }

			void pushBack(Data&& value) { data.emplace_back(makeBox<Data>(std::move(value))); }

			[[nodiscard]]
			RefT last() {
				return data.back().refMut();
			}

			[[nodiscard]]
			CRefT last() const {
				return data.back().ref();
			}

			/**
			 * Returns index of the last element (i.e. size - 1).
			 */
			[[nodiscard]]
			usize lastIndex() const {
				CORE_ASSERT(size() > 0, "Cannot get lastIndex() from empty BaseStableVector");
				return size() - 1;
			}

			template<class... Args>
			void emplaceBack(Args&&... args) {
				data.emplace_back(makeBox<Data>(std::forward<Args>(args)...));
			}

			class Iterator : public Secret<Iterator>::template Wrap<I> {
			private:
				using Impl = typename Secret<Iterator>::template Wrap<I>;
				friend Impl;
				friend BaseStableVector<Data>;
				I it;
				
				static constexpr Iterator factory(I&& u) {
					Iterator ans;
					ans.it = std::move(u);
					return ans;
				}
			public:
				using typename Impl::difference_type;
				using value_type = Data;
				using reference = value_type &;
				using pointer = value_type*;

				pointer operator->() const { return it->operator->(); }
				reference operator*() const { return **it; }
				reference operator[](difference_type diff) const { return *it[diff]; }
			};

			class ConstIterator : public Secret<ConstIterator>::template Wrap<CI> {
			private:
				using Impl = typename Secret<ConstIterator>::template Wrap<CI>;
				friend Impl;
				friend BaseStableVector<Data>;
				CI it;

				static constexpr ConstIterator factory(CI&& u) {
					ConstIterator ans;
					ans.it = std::move(u);
					return ans;
				}
			public:
				using typename Impl::difference_type;
				using value_type = Data;
				using reference = value_type const &;
				using pointer = value_type const *;

				pointer operator->() const { return it->operator->(); }
				reference operator*() const { return **it; }
				reference operator[](difference_type diff) const { return *it[diff]; }
			};

			Iterator begin() { return Iterator::factory(data.begin()); }
			
			ConstIterator begin() const { return ConstIterator::factory(data.cbegin()); }

			Iterator end() { return Iterator::factory(data.end()); }

			ConstIterator end() const { return ConstIterator::factory(data.cend()); }
		};

	}

	/**
	 * @brief Expandable list (like std::vector), but with stable references (References never
	 * become dangling, because actual data never moves).
	 *
	 * @tparam Key must be convertible to and from usize.
	 *
	 * @note Add stable range based iteration (Probably with indexes)
	 */
	template<class Data>
	class StableVector;

	/**
	 * @details Actually stores Data, but returns it as const on access.
	 */
	template<class Data>
	class StableVector<const Data>: private internal::BaseStableVector<Data> {
		using Base = internal::BaseStableVector<Data>;

		friend class StableVector<Data>;

		/**
		 * @brief Constructor form a base (without const) for further use in toConstData.
		 */
		explicit StableVector(Base&& base): Base{ std::move(base) } {}

	public:
		StableVector()               = default;
		StableVector(StableVector&&) = default;

		/**
		 * @note explicit delete here causes much better compiler errors.
		 * @note It is deleted because data member can't be copied in a simple way.
		 */
		StableVector(const StableVector&) = delete;

		StableVector<const Data> toConstData() && { return std::move(*this); }

		using Base::size, Base::empty, Base::notEmpty, Base::pushBack, Base::lastIndex,
			Base::emplaceBack;
		using RefT          = Base::CRefT;
		using CRefT         = Base::CRefT;
		using Iterator      = Base::ConstIterator;
		using ConstIterator = Base::ConstIterator;

		[[nodiscard]]
		CRefT operator[](usize pos) {
			return Base::operator[](pos);
		}

		[[nodiscard]]
		CRefT operator[](usize pos) const {
			return Base::operator[](pos);
		}

		[[nodiscard]]
		CRefT last() {
			return Base::last();
		}

		[[nodiscard]]
		CRefT last() const {
			return Base::last();
		}

		ConstIterator begin() const { return Base::begin(); }

		ConstIterator end() const { return Base::end(); }
	};

	template<class Data>
	class StableVector: private internal::BaseStableVector<Data> {
		using Base = internal::BaseStableVector<Data>;

	public:
		StableVector()               = default;
		StableVector(StableVector&&) = default;
		/**
		 * @note explicit delete here causes much better compiler errors.
		 * @note It is deleted because data member can't be copied in a simple way.
		 */
		StableVector(const StableVector&) = delete;

		using Base::size, Base::empty, Base::notEmpty, Base::operator[], Base::pushBack, Base::last,
			Base::lastIndex, Base::emplaceBack, Base::begin, Base::end;
		using RefT          = Base::RefT;
		using CRefT         = Base::CRefT;
		using Iterator      = Base::Iterator;
		using ConstIterator = Base::ConstIterator;

		/**
		 * @brief Converts stable vector into
		 * A stable vector storing the same objects but as const Data (instead of Data).
		 * It is useful when we want to "lock" the state of the objects stored in BaseStableVector.
		 * @note References created before this operation may still modify the data.
		 *
		 * The original container is left in an empty state, and should not be used again.
		 */
		StableVector<const Data> toConstData() && {
			return StableVector<const Data>{ std::move(*this) };
		}
	};

}
