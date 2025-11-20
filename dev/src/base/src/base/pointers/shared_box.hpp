#pragma once

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/default_deleter.hpp>

namespace base {
	template<class DataDeleter>
	class ControlBlock final {
	public:
        usize n_owners;
		[[no_unique_address]] DataDeleter data_deleter;

		ControlBlock() = delete;
		ControlBlock(DataDeleter deleter) noexcept: n_owners{ 1 }, data_deleter { std::move(deleter) } {}
    };

    template<class T, class Deleter = base::DefaultBoxPtrDeleter<T>>
    class SharedBox final {
    private:
        static_assert(
			IsPlainType<Deleter>,
			"Deleter must be a plain type (non reference, non pointer). This requirement is not "
			"expressed as a requires clause/concept "
			"usage, to prevent the need to write it in friend declarations."
			"See: http://en.cppreference.com/w/cpp/language/conflicting_declarations.html . This "
			"is especially important as some conflicting declaration errors are "
			"no-diagnostic-required cases on non matching requirement friend "
			"redefinition."
		);

		T* data_ptr;
        ControlBlock<Deleter>* ctrl_ptr;

		template<class U, class UDeleter>
		friend class SharedBox;

		constexpr void assertNotNull() const {
			if (data_ptr == nullptr) CORE_PANIC("SharedBox was in null state, when non-null was required!");
		}

		explicit SharedBox(T* ptr, ControlBlock<Deleter>* ctrl) noexcept: data_ptr{ ptr }, ctrl_ptr{ ctrl } {
			assertNotNull();
		}

		/**
		 * @brief Decrements the number of the owners of the object pointed to.
		 * If the counter reaches 0, deletes the object and the control block.
		*/
		void renounce_ownership() {
			ctrl_ptr->n_owners--;
			if (ctrl_ptr->n_owners == 0) {
				ctrl_ptr->data_deleter.del(data_ptr);
				delete ctrl_ptr;
			}
			data_ptr = nullptr;
			ctrl_ptr = nullptr;
		}
    public:
        SharedBox() = delete;
        SharedBox(std::nullptr_t) = delete;

        /**
		 * @brief Constructs a SharedBox from a raw pointer.
		 * It takes ownership of the pointer.
		 *
		 * For a regular construction use `makeSharedBox` instead.
		 * It is not a constructor in order to make this call more explicit.
		 */
		static SharedBox fromPointerWithCustomDeleter(T* ptr, Deleter deleter) noexcept {
			return SharedBox(ptr, new ControlBlock(deleter));
		}

		/**
		 * @brief Same as fromPointerWithCustomDeleter, but uses default constructed Deleter.
		 */
		static SharedBox fromPointer(T* ptr) noexcept {
			return fromPointerWithCustomDeleter(ptr, Deleter{});
		}

        SharedBox(const SharedBox& other) noexcept:
			  data_ptr{ other.data_ptr },
			  ctrl_ptr{ other.ctrl_ptr } {
				ctrl_ptr->n_owners++;
		}
		
		SharedBox(SharedBox&& other) noexcept = delete;

		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		SharedBox(SharedBox<U, UDeleter>&& other) = delete;

		/**
		 * @brief Copy assignment. The ownership of the object previously pointed to is renounced.
		 * The ownership of the object pointed to by `oth` is taken (the number of owners is increased).
		 * 
		 * @param other
		 * @return SharedBox&
		 */
		SharedBox& operator=(const SharedBox& other) noexcept {
			renounce_ownership();

			data_ptr = other.data_ptr;
			ctrl_ptr = other.ctrl_ptr;
			ctrl_ptr->n_owners++;
			return *this;
		}

		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		SharedBox& operator=(SharedBox<U, UDeleter>&& other) = delete;

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

		bool operator==(const SharedBox& other) const { return ctrl_ptr == other.ctrl_ptr; }

		~SharedBox() {
			renounce_ownership();
		}
    };

	/**
	 * @brief Constructs a SharedBox by forwarding the arguments to T constructor
	 * and allocating memory with new operator.
	 * @note default initialization of Deleter is used.
	 */
	template<class T, class Deleter = base::DefaultBoxPtrDeleter<T>, class... Args>
	inline SharedBox<T, Deleter> makeSharedBox(Args&&... args) {
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
		return SharedBox<T, Deleter>::fromPointerWithCustomDeleter(
			new T(std::forward<Args>(args)...), Deleter{}
		);
	}
}

using base::SharedBox;
using base::makeSharedBox;
