#pragma once

#include <base/comptime/is_complete.hpp>

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
}

/**
 * @brief Macro for declaring a specialization of DefaultBoxPtrDeleter for type T.
 * It should be used near the type declaration / forward declaration.
 * It has to be used in pair with DEFAULT_BOX_PTR_DELETER_DEFINITION(T).
 * See DefaultBoxPtrDeleter documentation for more details.
 *
 * @important It has to be used in top-level.
 *
 * @param T type for which the specialization is declared
 */
#define DEFAULT_BOX_PTR_DELETER_DECLARATION(T)                  \
	template<>                                                  \
	struct base::DefaultBoxPtrDeleter<T> final {                \
		DefaultBoxPtrDeleter() = default;                       \
                                                                \
		template<class U>                                       \
		DefaultBoxPtrDeleter(const DefaultBoxPtrDeleter<U>&) {} \
		static void del(T* ptr);                                \
	};

/**
 * @brief Macro for defining a specialization of DefaultBoxPtrDeleter for type T.
 * It should be used where type is complete.
 * It has to be used in pair with DEFAULT_BOX_PTR_DELETER_DECLARATION(T).
 * See DefaultBoxPtrDeleter documentation for more details.
 *
 * @important It has to be used in top-level.
 *
 * @param T type for which the specialization is declared
 */
#define DEFAULT_BOX_PTR_DELETER_DEFINITION(T)                                \
	void base::DefaultBoxPtrDeleter<T>::del(T* ptr) {                        \
		static_assert(IS_COMPLETE_V<T>, "T must be complete at this point"); \
		delete ptr;                                                          \
	}
