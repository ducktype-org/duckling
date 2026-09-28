#pragma once

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/misc/noexcept.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/default_deleter.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <atomic>
#include <concepts>
#include <type_traits>

namespace base {

	namespace internal {

		/**
		 * @brief Stores the number of owners and the deleter function.
		 *
		 * The control block derives from this base type so that `SharedBox<U>` can be
		 * converted to `SharedBox<T>` when the pointer types are compatible (for
		 * example `U` -> `const U`). Note that `ControlBlock<Derived>` is not
		 * convertible to `ControlBlock<Base>`, so storing the control block via a
		 * `BaseControlBlock` pointer enables the required conversions of
		 * `SharedBox` instances.
		 */
		struct BaseControlBlock {
			std::atomic<u64> n_owners                = 1;
			virtual void     del(void* ptr) noexcept = 0;
			virtual ~BaseControlBlock()              = default;
		};

		/**
		 * @brief Control block that stores the owner count and holds a deleter for T.
		 *
		 * This type implements `BaseControlBlock` and provides a concrete `del(void*)`
		 * override which forwards to the stored `Deleter`. The control block is
		 * allocated alongside the managed object and shared between `SharedBox`
		 * instances. Making the control block derive from `BaseControlBlock`
		 * enables converting `SharedBox<U>` to `SharedBox<T>` when the pointer
		 * types are compatible (e.g. `U` -> `const U`).
		 *
		 * @tparam T The pointed-to type.
		 * @tparam Deleter Deleter type used to destroy `T` (must provide `del(T*)`).
		 */
		template<class T, class Deleter = base::DefaultBoxPtrDeleter<T>>
		struct ControlBlock final: public BaseControlBlock {
			static_assert(IS_COMPLETE_V<T>, "Type T must be complete");
			static_assert(
				IsPlainType<Deleter>,
				"Deleter must be a plain type (non reference, non pointer). This requirement is "
				"not "
				"expressed as a requires clause/concept "
				"usage, to prevent the need to write it in friend declarations."
				"See: http://en.cppreference.com/w/cpp/language/conflicting_declarations.html . "
				"This "
				"is especially important as some conflicting declaration errors are "
				"no-diagnostic-required cases on non matching requirement friend "
				"redefinition."
			);
			static_assert(requires(Deleter d, T* p) { d.del(p); }, "Deleter must support d.del(T*)");

			[[no_unique_address]] Deleter deleter;

			void del(void* ptr) noexcept override { deleter.del(static_cast<T*>(ptr)); }

			ControlBlock(Deleter deleter): deleter(std::move(deleter)) {}
		};
	}

	/**
	 * @brief A pointer wrapper type, that shares the ownership of the pointer and deletes it
	 * when all of the owners go out of scope.
	 *
	 * \parallel Concurrent usage of different SharedBox instances owning the same object is safe,
	 * but concurrent usage of the same SharedBox instance is not. This mimics the behavior of
	 * std::shared_ptr.
	 *
	 * @note: An invariant is kept, that either both `data_ptr` and `ctrl_ptr` are either `nullptr`
	 * (which initially happens by move and can propagate through copying) or neither is `nullptr`.
	 *
	 * @note Currently deleters are supported in a simple, copy-based way. If the need for
	 * more complex behavior arises, we can add it as needed.
	 *
	 * @tparam T pointed type
	 * @tparam Deleter type used to delete the pointer, defaults to base::DefaultBoxPtrDeleter<T>.
	 * It has to define static method `void del(T*)`.
	 */
	template<class T>
	class SharedBox final {
	private:
		T*                          data_ptr;
		internal::BaseControlBlock* ctrl_ptr;

		constexpr void assertNotNull() const {
			if (data_ptr == nullptr || ctrl_ptr == nullptr)
				CORE_PANIC("SharedBox was in null state, when non-null was required!");
		}

		[[nodiscard]]
		constexpr bool isFullyNull() const {
			return (data_ptr == nullptr && ctrl_ptr == nullptr);
		}

		explicit SharedBox(T* ptr, internal::BaseControlBlock* ctrl) noexcept:
			  data_ptr{ ptr },
			  ctrl_ptr{ ctrl } {
			assertNotNull();
		}

		/**
		 * @brief: Decrements the number of the owners of the object pointed to.
		 * If the counter reaches 0, deletes the object and the control block.
		 * Sets the SharedBox to null state.
		 */
		void renounceOwnership() noexcept {
			if (isFullyNull()) return;
			assertNotNull();

			u64 n_owners_before = ctrl_ptr->n_owners.fetch_sub(1, std::memory_order_acq_rel);

			if (n_owners_before == 1) {
				// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
				// The deleter of the control block is typed on the non-const type the object was
				// created with, so a `SharedBox<const T>` has to drop the constness here.
				ctrl_ptr->del(const_cast<std::remove_const_t<T>*>(data_ptr));
				delete ctrl_ptr;
				// NOLINTEND(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
			}
			nullify();
		}

		/**
		 * @brief: Helper function to put the SharedBox into a null state.
		 */
		void nullify() noexcept {
			// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
			data_ptr = nullptr;
			ctrl_ptr = nullptr;
			// NOLINTEND(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
		}

	public:
		SharedBox()               = delete;
		SharedBox(std::nullptr_t) = delete;

		template<class U>
		friend class SharedBox;

		/**
		 * @brief Constructs a SharedBox from a raw pointer.
		 * It takes ownership of the pointer.
		 *
		 * For a regular construction use `makeSharedBox` instead.
		 * It is not a constructor in order to make this call more explicit.
		 */
		template<class Deleter>
		static SharedBox fromPointerWithCustomDeleter(T* ptr, Deleter deleter) noexcept {
			// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
			return SharedBox(ptr, new internal::ControlBlock<T, Deleter>(std::move(deleter)));
			// NOLINTEND(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
		}

		/**
		 * @brief Same as fromPointerWithCustomDeleter, but uses default constructed Deleter.
		 */
		static SharedBox fromPointer(T* ptr) noexcept {
			return fromPointerWithCustomDeleter(ptr, DefaultBoxPtrDeleter<T>{});
		}

		SharedBox(const SharedBox& other) noexcept:
			  data_ptr{ other.data_ptr },
			  ctrl_ptr{ other.ctrl_ptr } {
			if (isFullyNull()) return;
			assertNotNull();
			ctrl_ptr->n_owners.fetch_add(1, std::memory_order_acq_rel);
		}

		template<class U = T>
		requires std::convertible_to<U*, T*> SharedBox(const SharedBox<U>& other) noexcept:
			  data_ptr{ other.data_ptr },
			  ctrl_ptr{ other.ctrl_ptr } {
			if (isFullyNull()) return;
			assertNotNull();
			ctrl_ptr->n_owners.fetch_add(1, std::memory_order_acq_rel);
		}

		SharedBox(SharedBox&& other) noexcept:
			  data_ptr{ std::move(other).data_ptr },
			  ctrl_ptr{ std::move(other).ctrl_ptr } {
			if (isFullyNull()) return;
			assertNotNull();
			other.nullify();
		}

		template<class U = T>
		requires std::convertible_to<U*, T*> SharedBox(SharedBox<U>&& other) noexcept:
			  data_ptr{ std::move(other).data_ptr },
			  ctrl_ptr{ std::move(other).ctrl_ptr } {
			if (isFullyNull()) return;
			assertNotNull();
			other.nullify();
		}

		// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
		/**
		 * @brief Takes over the ownership of a non-null `Box`, together with its deleter.
		 * @note Do not try multiple inheritance with this SharedBox, it doesn't work there.
		 */
		SharedBox(Box<T>&& other) noexcept:
			  data_ptr{ std::move(other).ptr },
			  ctrl_ptr{ new internal::ControlBlock<T>(std::move(other).deleter) } {
			other.ptr = nullptr;
			assertNotNull();
		}

		// NOLINTEND(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)

		/**
		 * @brief Copy assignment. The ownership of the object previously pointed to is renounced.
		 * The ownership of the object pointed to by `oth` is taken (the number of owners is
		 * increased).
		 *
		 * @param other
		 * @return SharedBox&
		 */
		SharedBox& operator=(const SharedBox& other) noexcept {
			renounceOwnership();

			data_ptr = other.data_ptr;
			ctrl_ptr = other.ctrl_ptr;
			if (!isFullyNull()) {
				assertNotNull();
				ctrl_ptr->n_owners.fetch_add(1, std::memory_order_acq_rel);
			}
			return *this;
		}

		template<class U = T>
		requires std::convertible_to<U*, T*>
		SharedBox& operator=(const SharedBox<U>& other) noexcept {
			renounceOwnership();

			data_ptr = other.data_ptr;
			ctrl_ptr = other.ctrl_ptr;
			if (!isFullyNull()) {
				assertNotNull();
				ctrl_ptr->n_owners.fetch_add(1, std::memory_order_acq_rel);
			}
			return *this;
		}

		/**
		 * @brief Move assignment. The ownership of the object previously pointed to is renounced.
		 * The number of the owners stays the same.
		 */
		SharedBox& operator=(SharedBox&& other) noexcept {
			renounceOwnership();

			data_ptr = std::move(other).data_ptr;
			ctrl_ptr = std::move(other).ctrl_ptr;
			if (!isFullyNull()) {
				assertNotNull();
				other.nullify();
			}
			return *this;
		}

		template<class U = T>
		requires std::convertible_to<U*, T*> SharedBox& operator=(SharedBox<U>&& other) noexcept {
			renounceOwnership();

			data_ptr = std::move(other).data_ptr;
			ctrl_ptr = std::move(other).ctrl_ptr;
			if (!isFullyNull()) {
				assertNotNull();
				other.nullify();
			}
			return *this;
		}

		friend void swap(SharedBox& first, SharedBox& second) noexcept {
			std::swap(first.data_ptr, second.data_ptr);
			std::swap(first.ctrl_ptr, second.ctrl_ptr);
		}

		/**
		 * @brief Returns a mutable pointer to the pointed value, wrapped in Ref type.
		 *
		 * @return Ref<T>
		 */
		[[nodiscard]]
		Ref<T> refMut() const noexcept {
			return Ref<T>(data_ptr);
		}

		/**
		 * @brief Returns an immutable pointer to the pointed value, wrapped in Ref type.
		 *
		 * @return Ref<const T>
		 */
		[[nodiscard]]
		Ref<const T> ref() const noexcept {
			return Ref<const T>(data_ptr);
		}

		explicit operator bool() const {
			if (isFullyNull()) return false;
			assertNotNull();
			return true;
		}

		T* operator->() const {
			assertNotNull();
			return data_ptr;
		}

		T* get() const {
			assertNotNull();
			return data_ptr;
		}

		T& operator*() const {
			assertNotNull();
			return *data_ptr;
		}

		bool operator==(const SharedBox& other) const {
			CORE_ASSERT(
				(data_ptr == other.data_ptr) == (ctrl_ptr == other.ctrl_ptr),
				"Equality of control blocks should be equivalent to shared boxes owning the same "
				"object!"
			);
			return data_ptr == other.data_ptr;
		}

		/**
		 * @brief Resets the SharedBox to null state, renouncing the ownership of the object.
		 */
		void reset() noexcept { renounceOwnership(); }

		/**
		 * @brief Returns the number of SharedBox instances sharing ownership of the same object.
		 * If the SharedBox is in null state, returns 0.
		 */
		[[nodiscard]]
		u64 ownersCount() const noexcept {
			if (isFullyNull()) return 0;
			assertNotNull();

			// Relaxed ordering here is ok here, since it is only used to get an approximate number
			// of owners.
			return ctrl_ptr->n_owners.load(std::memory_order_relaxed);
		}

		~SharedBox() { renounceOwnership(); }
	};

	/**
	 * @brief Constructs a SharedBox by forwarding the arguments to T constructor
	 * and allocating memory with new operator.
	 * @note default initialization of Deleter is used.
	 */
	template<class T, class Deleter = base::DefaultBoxPtrDeleter<T>, class... Args>
	inline SharedBox<T> makeSharedBox(Args&&... args) {
		static_assert(
			std::is_default_constructible_v<Deleter>,
			"Deleter must be default constructible."
			"This requirement is not "
			"expressed as a requires clause/concept "
			"usage, to prevent the need to write it in friend declarations."
			"See: http://en.cppreference.com/w/cpp/language/conflicting_declarations.html . This "
			"is especially important as some conflicting declaration errors are "
			"no-diagnostic-required cases on non matching requirement friend "
			"redefinition."

		);

		// NOLINTBEGIN(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
		return SharedBox<T>::fromPointerWithCustomDeleter(
			new T(std::forward<Args>(args)...), Deleter{}
		);
		// NOLINTEND(clang-analyzer-cplusplus.NewDelete,clang-analyzer-cplusplus.NewDeleteLeaks)
	}

	template<class T>
	using CSharedBox = SharedBox<const T>;
}

using base::CSharedBox;
using base::makeSharedBox;
using base::SharedBox;
