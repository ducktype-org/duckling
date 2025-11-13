#pragma once

// #include <atomic>

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/pointers/ref.hpp>
#include <base/pointers/default_deleter.hpp>

namespace base {
	template<class DataDeleter, class ControlBlockDeleter>
	class ControlBlock final {
	public:
        usize n_owners;
		[[no_unique_address]] DataDeleter data_deleter;
		ControlBlock() = delete;
		ControlBlock(Deleter deleter) noexcept: n_owners{ 0 }, deleter { std::move(deleter) } {}
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

		//template<class U, class UDeleter>
		//friend class MBox;

		constexpr void assertNotNull() const {
			if (data_ptr == nullptr) CORE_PANIC("Box was in null state, when non-null was required!");
		}

		explicit SharedBox(T* ptr, ControlBlock<Deleter>* ctrl) noexcept: data_ptr{ ptr }, ctrl_ptr{ ctrl } {
			assertNotNull();
		}
    public:
        SharedBox() = delete;
        SharedBox(std::nullptr_t) = delete;

        /**
		 * @brief Constructs a SharedBox from a raw pointer.
		 * It takes ownership of the pointer.
		 *
		 * For a regular construction use `makeBox` instead.
		 * It is not a constructor in order to make this call more explicit.
		 */
		static SharedBox fromPointerWithCustomDeleter(T* ptr, Deleter deleter) noexcept {
			return SharedBox(ptr, *(new ControlBlock(deleter)));
		}

		/**
		 * @brief Same as fromPointerWithCustomDeleter, but uses default constructed Deleter.
		 */
		static SharedBox fromPointer(T* ptr) noexcept {
			return fromPointerWithCustomDeleter(ptr, Deleter{});
		}

        SharedBox(const SharedBox& other) = delete;

		/**
		 * @brief Assignment increments counter in the control block.
		 *
		 * @tparam U
		 * @param oth
		 * @return SharedBox&
		 */
		template<class U, class UDeleter>
		requires std::is_same<Deleter, UDeleter&&>
		SharedBox& operator=(Box<U, UDeleter>&& oth) noexcept {
			this.renounce_ownership();

			data_ptr = std::move(oth).data_ptr;
			ctlr_ptr = std::move(oth).ctrl_ptr;
			ctrl_ptr->n_owners++;
		}

		void renounce_ownership() {
			ctrl_ptr->n_owners--;
			if (ctrl_ptr->n_owners == 0) {
				ctrl_ptr->deleter.del(data_ptr);
				data_ptr = nullptr;

				delete ctrl_ptr;
				ctrl_ptr = nullptr;
			}
		}
    };
}

using base::SharedBox;