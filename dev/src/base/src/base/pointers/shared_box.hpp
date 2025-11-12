#pragma once

#include <atomic>

#include <base/comptime/is_complete.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/pointers/ref.hpp>

namespace base {

	/**
	 * @brief Default deleter functor used by Box, MBox.
	 *
	 * @note Adding specialization for custom types with macros
	 * DEFAULT_BOX_PTR_DELETER_DECLARATION(T) and
	 * DEFAULT_BOX_PTR_DELETER_DEFINITION(T) is supported and
	 * can be used to avoid delete on incomplete types.
	 * This effectively moves the definition into the cpp file, where the type is complete.
	 *
	 * @tparam T
	 */
	template<class T>
	struct DefaultBoxPtrDeleter final {
		DefaultBoxPtrDeleter() = default;

		/**
		 * DefaultBoxPtrDeleter can be constructed from other DefaultBoxPtrDeleter.
		 */
		template<class U>
		DefaultBoxPtrDeleter(const DefaultBoxPtrDeleter<U>&) {}

		static void del(T* ptr) {
			static_assert(
				IS_COMPLETE_V<T>,
				"DefaultBoxPtrDeleter can be used only with complete types. If you need to use it "
				"with "
				"incomplete type, please provide a specialization using macros "
				"DEFAULT_BOX_PTR_DELETER_DECLARATION(T) and DEFAULT_BOX_PTR_DELETER_DEFINITION(T)."
			);

			delete ptr;
		}
	};

    class ControlBlock final {
    private:
        std::atomic<usize> n_owners;
        std::atomic<usize> n_weak;
        std::atomic<bool> is_owned_mutably;
    public:
        explicit ControlBlock() noexcept: n_owners{ 1 }, n_weak{ 0 }, is_owned_mutably{ false } {}
        void addOwner() {
            n_owners++;
        }
        void removeOwner() {
            n_owners--;
        }
        void addWeak() {
            n_weak++;
        }
        void removeWeak() {
            n_weak--;
        }
        bool tryTakeMut() {
            return !is_owned_mutably.exchange(true);
        }
    };

    template<class T, class Deleter = DefaultBoxPtrDeleter<T>>
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
        ControlBlock* ctrl_ptr;
		[[no_unique_address]] Deleter deleter;

		template<class U, class UDeleter>
		friend class SharedBox;

		//template<class U, class UDeleter>
		//friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("Box was in null state, when non-null was required!");
		}

		explicit SharedBox(T* ptr, ControlBlock* ctrl, Deleter deleter) noexcept: data_ptr{ ptr }, ctrl_ptr{ ctrl }, deleter{ std::move(deleter) } {
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
			return SharedBox(ptr, *(new ControlBlock()) std::move(deleter));
		}

		/**
		 * @brief Same as fromPointerWithCustomDeleter, but uses default constructed Deleter.
		 */
		static SharedBox fromPointer(T* ptr) noexcept {
			return fromPointerWithCustomDeleter(ptr, Deleter{});
		}

        SharedBox(const SharedBox& other) = delete;
    };
}

using base::SharedBox;