/**
 * @file stable_container.hpp
 * @brief Provides a simple expandable container with stable references.
 */
#pragma once

#include "ref.hpp"

#include "base/exceptions.hpp"
#include "base/ints.hpp"

#include <deque>
#include <iterator>
#include <vector>

namespace base {
	namespace internal {
		/**
		 * @class BaseStableVector
		 * @brief Wrapper class over a container (std::vector<Box>) which returns Ref/CRef on access.
		 * @details It is used to allow a seamless conversion from StableVector<Data> to
		 * StableVector<const Data>.
		 */
		template<class Data>
		class BaseStableVector {
		private:
			using father_t = BaseStableVector;

			class Node {
			private:
				friend father_t;
				usize               idx;
				father_t&           father;
				std::optional<Data> content;

			public:
				template<typename... Args>
				Node(usize p, father_t& f, Args&&... args):
					  idx(p),
					  father(f),
					  content(std::in_place, std::forward<Args>(args)...) {}
			};

			class StableAlocator{
			private:
				union Wrap {
					Data val;
					Wrap() {};
					~Wrap() {};
				};
				
				std::vector<usize> available;
				std::deque<Wrap> data;
				std::vector<bool> allocated;	
			public:

				StableAlocator() = default;
				
				~StableAlocator() {
					CORE_ASSERT(data.size() == allocated.size(), "sanity check, each cell must have corresponding bit if data is used");
					for (usize i = 0; i < data.size(); i++)
						if (allocated[i])
							dealloc(i);	
				}
				
				template<typename... Args>
				usize alloc(Args&&... args) {
					CORE_ASSERT(data.size() == allocated.size(), "sanity check, each cell must have corresponding bit if data is used");

					if (available.empty()) {
						available.push_back(data.size());
						data.emplace_back();
						allocated.push_back(false);
					}

					usize alloc_idx = available.back();
					available.pop_back();

					CORE_ASSERT(allocated.at(alloc_idx) == false, "trying to allocate space which is already in use");
					allocated.at(alloc_idx) = true;
					
					new (&data.at(alloc_idx).val) Data(std::forward<Args>(args)...);
					
					return alloc_idx;
				}
				
				void dealloc(usize alloc_idx) {
					CORE_ASSERT(data.size() == allocated.size(), "sanity check, each cell must have corresponding bit if data is used");
					
					CORE_ASSERT(allocated.at(alloc_idx) == true, "trying to deallocate already empty space");
					allocated.at(alloc_idx) = false;
					
					data.at(alloc_idx).val.~Data();
					available.push_back(alloc_idx);
				}
				
				Data& at(usize alloc_idx) {
					CORE_ASSERT(data.size() == allocated.size(), "sanity check, each cell must have corresponding bit if data is used");
					CORE_ASSERT(allocated.at(alloc_idx) == true, "trying to get uninitialized data");
					return data.at(alloc_idx).val;
				}
			};

			using ContainerT = std::vector<Node*>;
			ContainerT data;
			static_assert(
				std::is_same_v<typename ContainerT::size_type, usize>,
				"When this fail, figure out what to do."
			);

			using AllocatedT = std::deque<Node>;
			AllocatedT nodes;
			ContainerT available;

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
				return RefT{ &data.at(pos)->content.value() };
			}

			[[nodiscard]]
			CRefT operator[](usize pos) const {
				return CRefT{ &data.at(pos)->content.value() };
			}

			template<class... Args>
			void emplaceBack(Args&&... args) {
				if (available.empty()) {
					nodes.emplace_back(data.size(), *this, std::forward<Args>(args)...);
					data.push_back(&nodes.back());
					return;
				}

				Node* place = available.back();
				available.pop_back();

				place->idx = data.size();
				place->content.emplace(std::forward<Args>(args)...);
				data.push_back(place);
			}

			void pushBack(const Data& value) { emplaceBack(value); }

			void pushBack(Data&& value) { emplaceBack(std::move(value)); }

			[[nodiscard]]
			RefT last() {
				return RefT{ &data.back()->content.value() };
			}

			[[nodiscard]]
			CRefT last() const {
				return CRefT{ &data.back()->content.value() };
			}

			/**
			 * Returns index of the last element (i.e. size - 1).
			 */
			[[nodiscard]]
			usize lastIndex() const {
				CORE_ASSERT(size() > 0, "Cannot get lastIndex() from empty BaseStableVector");
				return size() - 1;
			}

		private:
			/**
			 * @class Wrap
			 * @brief this is the class from which BaseCRTP should publicly inherit random access
			 * operations (iterator boilerplate)
			 * @tparam ValT - type of hel value (used to determine iterator traits)
			 * @tparam BaseCRTP - a base type for
			 * [CRTP](https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/)
			 */
			template<typename BaseCRTP, typename ValT>
			class Wrap {
			private:
				Wrap() = default;
				friend BaseCRTP;
				friend father_t;

				static constexpr u64 getPos(const BaseCRTP& iter) {
					return iter.it == nullptr ? iter.father->size() : iter.it->idx;
				}

				static constexpr BaseCRTP factory(BaseCRTP& iter, i64 diff) {
					BaseCRTP    ans  = iter;
					u64         pos  = getPos(iter) + diff;
					const auto& data = iter.father->data;

					iter.it = (pos < data.size()) ? data[pos] : nullptr;

					return ans;
				}

				static constexpr void assertValid(const BaseCRTP& iter) {
					CORE_ASSERT(
						iter.it != nullptr, "iterator is end() of container; don't dereference"
					);

					const auto& node = *iter.it;
					CORE_ASSERT(
						iter.father == &node.father,
						"value belongs to different container than iterator"
					);
					CORE_ASSERT(
						node.content.has_value(), "no value is stored"
					);  // <- Could be removed for performance

					const auto& data = iter.father->data;
					CORE_ASSERT(node.idx < data.size(), "iterator is outside the bounds");
					CORE_ASSERT(iter.it == data[node.idx], "container and iterator don't match");
				}

			public:
				using difference_type   = i64;
				using iterator_category = std::random_access_iterator_tag;
				using value_type        = std::remove_cvref_t<ValT>;
				using reference         = ValT&;
				using pointer           = ValT*;

				BaseCRTP& operator++(this BaseCRTP& self) {
					factory(self, 1);
					return self;
				}

				BaseCRTP operator++(this BaseCRTP& self, int) { return factory(self, 1); }

				BaseCRTP& operator--(this BaseCRTP& self) {
					factory(self, -1);
					return self;
				}

				BaseCRTP operator--(this BaseCRTP& self, int) { return factory(self, -1); }

				BaseCRTP& operator+=(this BaseCRTP& self, difference_type diff) {
					factory(self, diff);
					return self;
				}

				BaseCRTP operator+(this BaseCRTP self, const difference_type diff) {
					return self += diff;
				}

				friend BaseCRTP operator+(const difference_type diff, BaseCRTP iter) {
					return iter += diff;
				}

				BaseCRTP& operator-=(this const BaseCRTP& self, difference_type diff) {
					factory(self, -diff);
					return self;
				}

				BaseCRTP operator-(this BaseCRTP self, const difference_type diff) {
					return self -= diff;
				}

				difference_type operator-(this const BaseCRTP& self, const BaseCRTP& other) {
					return getPos(self) - getPos(other);
				}

				bool operator==(this const BaseCRTP& self, const BaseCRTP& other) {
					return (self - other) == 0;
				}

				auto operator<=>(this const BaseCRTP& self, const BaseCRTP& other) {
					return (self - other) <=> 0;
				}

				pointer operator->(this const BaseCRTP& self) {
					assertValid(self);
					return &self.it->content.value();
				}

				reference operator*(this const BaseCRTP& self) {
					assertValid(self);
					return self.it->content.value();
				}

				reference operator[](this const BaseCRTP& self, difference_type diff) {
					return *(self + diff);
				}
			};

		public:
			class Iterator: public Wrap<Iterator, Data> {
			private:
				friend Wrap<Iterator, Data>;
				friend father_t;
				Node*     it;
				father_t* father;

				static Iterator make(Node* iter, father_t* f) {
					Iterator ans;
					ans.it     = iter;
					ans.father = f;
					return ans;
				}
			};

			friend Wrap<Iterator, Data>;

			class ConstIterator: public Wrap<ConstIterator, const Data> {
			private:
				friend Wrap<ConstIterator, const Data>;
				friend father_t;
				Node*           it;
				const father_t* father;

				static ConstIterator make(Node* iter, const father_t* f) {
					ConstIterator ans;
					ans.it     = iter;
					ans.father = f;
					return ans;
				}
			};

			friend Wrap<ConstIterator, const Data>;

			/**
			 * @brief erase function
			 * @note don't pass the end() or Iterators from other containers
			 * @param del
			 * @return Iterator to the new position
			 */
			Iterator erase(Iterator del) {
				Iterator::Wrap::assertValid(del);
				CORE_ASSERT(del.father == this, "was given iterator of other container");

				u64 pos = Iterator::getPos(del);
				for (u64 i = pos; i < data.size(); ++i) data[i]->idx--;
				data.erase(data.begin() + pos);

				auto del_node = del.it;
				del_node->content.reset();
				available.push_back(del_node);

				auto new_ptr = (pos + 1 < data.size()) ? data[pos] : nullptr;

				return Iterator::make(new_ptr, this);
			}

			Iterator begin() { return Iterator::make(size() ? data[0] : nullptr, this); }

			ConstIterator begin() const {
				return ConstIterator::make(size() ? data[0] : nullptr, this);
			}

			Iterator end() { return Iterator::make(nullptr, this); }

			ConstIterator end() const { return ConstIterator::make(nullptr, this); }
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
			Base::emplaceBack, Base::erase;
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
			Base::lastIndex, Base::emplaceBack, Base::begin, Base::end, Base::erase;
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
