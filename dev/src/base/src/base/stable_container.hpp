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
#include <type_traits>
#include <vector>

namespace base {
	namespace internal {

		template<typename ObjT>
		class StableAlocator {
		private:
			std::vector<usize>              available;
			std::deque<std::optional<ObjT>> data;

		public:
			[[nodiscard]] bool isAllocated(usize alloc_idx) const {
				return data.at(alloc_idx).has_value();
			}

			template<typename... Args>
			usize alloc(Args&&... args) {
				if (available.empty()) {
					available.push_back(data.size());
					data.emplace_back(std::nullopt);
				}

				usize alloc_idx = available.back();
				available.pop_back();

				CORE_ASSERT(
					isAllocated(alloc_idx) == false,
					"trying to allocate space which is already in use"
				);
				data.at(alloc_idx).emplace(std::forward<Args>(args)...);

				return alloc_idx;
			}

			void dealloc(usize alloc_idx) {
				CORE_ASSERT(
					isAllocated(alloc_idx) == true, "trying to deallocate already empty space"
				);

				data.at(alloc_idx).reset();

				available.push_back(alloc_idx);
			}

			ObjT& at(usize alloc_idx) {
				CORE_ASSERT(isAllocated(alloc_idx) == true, "trying to get uninitialized data");
				return data.at(alloc_idx).value();
			}
		};

		/**
		 * @class BaseStableVector
		 * @brief Wrapper class over a container (std::vector<Box>) which returns Ref/CRef on access.
		 * @details It is used to allow a seamless conversion from StableVector<Data> to
		 * StableVector<const Data>.
		 */
		template<class Data>
		class BaseStableVector {
		private:
			using self_t = BaseStableVector;

			class Node final {
			private:
				friend self_t;
				usize               pos;
				const self_t*  node_father;
				Data                content;

			public:
				template<typename... Args>
				Node(usize p, const self_t* f, Args&&... args):
					  pos(p),
					  node_father(f),
					  content(std::forward<Args>(args)...) {}
			};

			using ContainerT = std::vector<usize>;
			ContainerT data;
			static_assert(
				std::is_same_v<typename ContainerT::size_type, usize>,
				"When this fail, figure out what to do."
			);

			mutable StableAlocator<Node> memory;

			inline Node& nodeAt(usize vec_pos) const { return memory.at(data.at(vec_pos)); }

		public:
			using RefT  = Ref<Data>;
			using CRefT = CRef<Data>;

			BaseStableVector() = default;

			BaseStableVector(BaseStableVector&& oth) noexcept:
				  data(std::move(oth.data)),
				  memory(std::move(oth.memory))  {
				for (int i = 0; i < data.size(); i++) {
					nodeAt(i).node_father = this;
				}
			}

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
				return RefT{ &nodeAt(pos).content };
			}

			[[nodiscard]]
			CRefT operator[](usize pos) const {
				return CRefT{ &nodeAt(pos).content };
			}

			template<class... Args>
			void emplaceBack(Args&&... args) {
				data.push_back(memory.alloc(size(), this, std::forward<Args>(args)...));
			}

			void pushBack(const Data& value) { emplaceBack(value); }

			void pushBack(Data&& value) { emplaceBack(std::move(value)); }

			/**
			 * Returns index of the last element (i.e. size - 1).
			 */
			[[nodiscard]]
			usize lastIndex() const {
				CORE_ASSERT(size() > 0, "Cannot get lastIndex() from empty BaseStableVector");
				return size() - 1;
			}

			[[nodiscard]]
			RefT last() {
				return RefT{ &nodeAt(lastIndex()).content };
			}

			[[nodiscard]]
			CRefT last() const {
				return CRefT{ &nodeAt(lastIndex()).content };
			}

		private:
			/**
			 * @class BaseIterator
			 * @brief this is the class which implements both iterator and const_iterator
			 * @tparam ValT - type of held value (used to determine iterator traits)
			 */
			template<typename ValT>
			requires std::is_same_v<std::remove_cvref_t<ValT>, Data> class BaseIterator {
			public:
				using difference_type   = i64;
				using iterator_category = std::random_access_iterator_tag;
				using value_type        = std::remove_cvref_t<ValT>;
				using reference         = ValT&;
				using pointer           = ValT*;

				BaseIterator() = default;

			private:
				Node*         inner;
				const self_t* iter_father;

				friend self_t;

				[[nodiscard]] inline usize getPos() const {
					return inner == nullptr ? iter_father->size() : inner->pos;
				}

				BaseIterator incThisRetOld(difference_type diff) {
					BaseIterator  ans       = *this;
					usize         pos       = getPos() + diff;
					const self_t& my_father = *iter_father;

					inner = (pos < my_father.size()) ? &my_father.nodeAt(pos) : nullptr;

					return ans;
				}

				static BaseIterator make(Node* iter, const self_t* f) {
					BaseIterator ans;
					ans.inner       = iter;
					ans.iter_father = f;
					return ans;
				}

				void assertValid() const {
					CORE_ASSERT(
						inner != nullptr, "iterator is end() of container; don't dereference"
					);

					const Node&   node        = *inner;
					const self_t& node_father = *node.node_father;
					usize         pos         = node.pos;
					CORE_ASSERT(
						iter_father == &node_father,
						"value belongs to different container than iterator"
					);

					CORE_ASSERT(
						inner == &node_father.nodeAt(pos), "container and iterator don't agree"
					);
				}

			public:
				BaseIterator& operator++() {
					incThisRetOld(1);
					return *this;
				}

				BaseIterator operator++(int) { return incThisRetOld(1); }

				BaseIterator& operator--() {
					incThisRetOld(-1);
					return *this;
				}

				BaseIterator operator--(int) { return incThisRetOld(-1); }

				BaseIterator& operator+=(difference_type diff) {
					incThisRetOld(diff);
					return *this;
				}

				BaseIterator operator+(const difference_type diff) const {
					return BaseIterator(*this) += diff;
				}

				friend BaseIterator operator+(const difference_type diff, BaseIterator oth) {
					return BaseIterator(oth) += diff;
				}

				BaseIterator& operator-=(difference_type diff) {
					incThisRetOld(-diff);
					return *this;
				}

				BaseIterator operator-(const difference_type diff) const {
					return BaseIterator(*this) -= diff;
				}

				difference_type operator-(const BaseIterator& oth) const {
					return getPos() - oth.getPos();
				}

				bool operator==(this const BaseIterator& self, const BaseIterator& other) {
					return (self - other) == 0;
				}

				auto operator<=>(this const BaseIterator& self, const BaseIterator& other) {
					return (self - other) <=> 0;
				}

				pointer operator->() const {
					assertValid();
					return &inner->content;
				}

				reference operator*() const {
					assertValid();
					return inner->content;
				}

				reference operator[](difference_type diff) const { return *(*this + diff); }
			};

		public:
			using Iterator      = BaseIterator<Data>;
			using ConstIterator = BaseIterator<const Data>;

			/**
			 * @brief erase function
			 * @note don't pass the end() or Iterators from other containers
			 * @param del
			 * @return Iterator to the new position
			 */
			Iterator erase(ConstIterator del) {
				del.assertValid();
				CORE_ASSERT(del.iter_father == this, "was given iterator of other container");

				usize u_vpos    = del.getPos();
				usize alloc_idx = data.at(u_vpos);

				for (usize i = u_vpos; i < size(); ++i) nodeAt(i).pos--;

				i64 s_vpos = static_cast<i64>(u_vpos);
				data.erase(data.begin() + s_vpos);

				memory.dealloc(alloc_idx);

				return begin() + s_vpos;
			}

			Iterator erase(Iterator del) {
				ConstIterator del2 = ConstIterator::make(del.inner, del.iter_father);

				return erase(del2);
			}

			Iterator begin() { return Iterator::make(size() ? &nodeAt(0) : nullptr, this); }

			ConstIterator begin() const {
				return ConstIterator::make(size() ? &nodeAt(0) : nullptr, this);
			}

			Iterator end() { return Iterator::make(nullptr, this); }

			ConstIterator end() const { return ConstIterator::make(nullptr, this); }

			/**
			 * @brief retrieval of Iterator from a Ref to held value
			 * @note UB when passing reference not allocated via Stable Vector
			 * @param ref
			 * @return Iterator
			 */
			static Iterator fromRef(Ref<Data> ref) requires std::is_standard_layout_v<Node> {
				// @TODO, there might be a better way to do it with C++26 reflections
				static constexpr auto offset = offsetof(Node, content);

				Node* node_ptr
					= reinterpret_cast<Node*>(reinterpret_cast<byte*>(ref.get()) - offset);
				const self_t* father = node_ptr->node_father;

				return Iterator::make(node_ptr, father);
			}
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
