#pragma once

#include "is_complete.hpp"
#include "ref.hpp"
#include "type_traits.hpp"

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

	/**
	 * @brief A pointer wrapper type, that owns the pointer and deletes it when it goes out of
	 * scope. It is not nullable, and it is not copyable.
	 * @note: When performing a move operation, the source pointer is set to nullptr.
	 * Attempt to use it after that will result in a panic. In the future we might consider
	 * removing this check in release build for performance.
	 *
	 * @note Currently deleters are supported if a simple, copy-based way. If the need for
	 * stranger behavior arises, we can add it as needed.
	 *
	 * @tparam T pointed type
	 * @tparam Deleter type used to delete the pointer, defaults to DefaultBoxPtrDeleter<T>. It has
	 * to define static method `void del(T*)`.
	 */
	template<class T, class Deleter = DefaultBoxPtrDeleter<T>>
	class Box final {
	private:
		static_assert(
			DirectType<Deleter>,
			"Deleter must be a direct type. This is not present as a requires clause/concept "
			"usage, to prevent no-diagnostic-required cases on non matching requirement friend "
			"redefinition."
		);

		T*                            ptr;
		[[no_unique_address]] Deleter deleter;

		template<class U, class UDeleter>
		friend class Box;

		template<class U, class UDeleter>
		friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("Box was in null state, when non-null was required!");
		}

		explicit Box(T* ptr, Deleter deleter) noexcept: ptr{ ptr }, deleter{ std::move(deleter) } {
			assertNotNull();
		}

	public:
		Box()               = delete;
		Box(std::nullptr_t) = delete;

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * It takes ownership of the pointer.
		 *
		 * For a regular construction use `makeBox` instead.
		 * It is not a constructor in order to make this call more explicit.
		 */
		static Box fromPointer(T* ptr, Deleter deleter) noexcept {
			return Box(ptr, std::move(deleter));
		}

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * It takes ownership of the pointer.
		 *
		 * For a regular construction use `makeBox` instead.
		 * It is not a constructor in order to make this call more explicit.
		 */
		static Box fromPointerWithDefaultDeleter(T* ptr) noexcept {
			return fromPointer(ptr, Deleter{});
		}

		Box(const Box& other) = delete;

		Box(Box&& other) noexcept:
			  ptr{ std::move(other).ptr },
			  deleter{ std::move(other).deleter } {
			other.ptr = nullptr;
		}

		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		Box(Box<U, UDeleter>&& other) noexcept:
			  ptr{ std::move(other).ptr },
			  deleter{ std::move(other).deleter } {
			other.ptr = nullptr;
		}

		Box& operator=(const Box& other) = delete;

		/**
		 * @brief Move assignment. The object previously pointed to by the Box is deleted.
		 *
		 * @tparam U
		 * @param oth
		 * @return Box&
		 */
		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		Box& operator=(Box<U, UDeleter>&& oth) noexcept {
			deleter.del(ptr);

			ptr     = std::move(oth).ptr;
			deleter = std::move(oth).deleter;

			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(Box& first, Box& second) noexcept { std::swap(first.ptr, second.ptr); }

		/**
		 * @brief Returns a mutable pointer to the pointed value, wrapped in Ref type.
		 *
		 * @return Ref<T>
		 */
		[[nodiscard]]
		Ref<T> refMut() const noexcept {
			return Ref<T>(ptr);
		}

		/**
		 * @brief Returns an immutable pointer to the pointed value, wrapped in Ref type.
		 *
		 * @return Ref<const T>
		 */
		[[nodiscard]]
		Ref<const T> ref() const noexcept {
			return Ref<const T>(ptr);
		}

		T* operator->() const {
			assertNotNull();
			return ptr;
		}

		T& operator*() const {
			assertNotNull();
			return *ptr;
		}

		bool operator==(const Box& other) const { return ptr == other.ptr; }

		~Box() { deleter.del(ptr); }
	};

	/**
	 * @brief A nullable pointer wrapper type, that owns the pointer and deletes it when it goes out
	 * of scope. Implements both null-unchecked and null-checked access to the pointer. It is not
	 * copyable.
	 * @note: When attempting to use a pointer when it is in null state, a panic will be thrown. In
	 * the future we might consider removing this check in release build for performance.
	 *
	 * @note Currently only stateless deleters are supported. If the need for stateful deleters
	 * arises, we can add support for them by storing the deleter instance in the Box (e.g.
	 * [no_unique_address]] Deleter deleter;).
	 *
	 * @tparam T pointed type
	 * @tparam Deleter type used to delete the pointer, defaults to DefaultBoxPtrDeleter<T>. It has
	 * to define static method `void del(T*)`.
	 */
	template<class T, class Deleter = DefaultBoxPtrDeleter<T>>
	class MBox final {
	private:
		static_assert(
			DirectType<Deleter>,
			"Deleter must be a direct type. This is not present as a requires clause/concept "
			"usage, to prevent no-diagnostic-required cases on non matching requirement friend "
			"redefinition."
		);

		T*                            ptr = nullptr;
		[[no_unique_address]] Deleter deleter;

		template<class U, class UDeleter>
		friend class MBox;

		template<class U, class UDeleter>
		friend class Box;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("MBox was in null state, when non-null was required!");
		}

		/**
		 * @brief Constructs an MBox from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit MBox(T* ptr, Deleter deleter = Deleter{}) noexcept:
			  ptr{ ptr },
			  deleter{ std::move(deleter) } {}


	public:
		MBox() = default;

		MBox(std::nullptr_t) {}

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept:
			  ptr{ std::move(other).ptr },
			  deleter{ std::move(other).deleter } {
			other.ptr = nullptr;
		}

		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		MBox(Box<U, UDeleter>&& other) noexcept:
			  ptr{ std::move(other).ptr },
			  deleter{ std::move(other).deleter } {
			other.ptr = nullptr;
		}

		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		MBox(MBox<U, UDeleter>&& other) noexcept:
			  ptr{ std::move(other).ptr },
			  deleter{ std::move(other).deleter } {
			other.ptr = nullptr;
		}

		MBox& operator=(const MBox& other) = delete;

		/**
		 * @brief Move assignment. The object previously pointed to by the MBox is deleted.
		 *
		 * @tparam U
		 * @param oth
		 * @return MBox&
		 */
		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		MBox& operator=(MBox<U, UDeleter>&& oth) noexcept {
			deleter.del(ptr);

			ptr     = std::move(oth).ptr;
			deleter = std::move(oth).deleter;

			oth.ptr = nullptr;
			return *this;
		}

		/**
		 * @brief Move assignment. The object previously pointed to by the MBox is deleted.
		 *
		 * @tparam U
		 * @param oth
		 * @return MBox&
		 */
		template<class U, class UDeleter>
		requires std::is_constructible_v<Deleter, UDeleter&&>
		MBox& operator=(Box<U, UDeleter>&& oth) noexcept {
			deleter.del(ptr);

			ptr     = std::move(oth).ptr;
			deleter = std::move(oth).deleter;

			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(MBox& first, MBox& second) noexcept { std::swap(first.ptr, second.ptr); }

		operator bool() const { return ptr; }

		/**
		 * @brief Returns a mutable pointer to the pointed value, wrapped in MRef type.
		 *
		 * @return MRef<T>
		 */
		[[nodiscard]]
		MRef<T> refMut() const noexcept {
			return MRef<T>(ptr);
		}

		/**
		 * @brief Returns a immutable pointer to the pointed value, wrapped in MRef type.
		 *
		 * @return MRef<const T>
		 */
		[[nodiscard]]
		MRef<const T> ref() const noexcept {
			return MRef<T>(ptr);
		}

		/**
		 * @brief Null checked access method. Returns optional Ref to the pointed value.
		 * If MBox was in null state, the optional will be empty.
		 * If MBox was not in null state, the optional will contain Ref to the pointed value.
		 * Can be nicely used with optional pattern matching from base.
		 *
		 * @return Optional<Ref<T>>
		 */
		[[nodiscard]]
		constexpr Optional<Ref<T>> toOpt() const noexcept {
			if (ptr == nullptr) return {};
			return Ref<T>(ptr);
		}

		/**
		 * @brief Null unchecked access method. Works like "->" operator on typical pointer.
		 * @note panics if MBox was in null state.
		 */
		T* operator->() const {
			assertNotNull();
			return ptr;
		}

		/**
		 * @brief Null unchecked access method. Works like "*" operator on typical pointer.
		 * @note panics if MBox was in null state.
		 */
		T& operator*() const {
			assertNotNull();
			return *ptr;
		}

		/**
		 * @brief Method that converts MBox to Optional<Box>.
		 * It leaves MBox in null state.
		 *
		 * @return Optional<SDBox<T>>
		 */
		Optional<Box<T, Deleter>> toOptBox() && {
			T* output = ptr;
			ptr       = nullptr;
			if (output == nullptr) return {};
			return Box<T, Deleter>::fromPointer(output, std::move(deleter));
		}

		~MBox() { deleter.del(ptr); }
	};

	// Deduction guide for constructing a MBox from a Box:
	template<class U, class UDeleter>
	MBox(Box<U, UDeleter>&&) noexcept -> MBox<U, UDeleter>;

	/**
	 * @brief Constructs a Box by forwarding the arguments to T constructor
	 * and allocating memory with new operator.
	 * @note default initialization of Deleter is used.
	 */
	template<class T, class Deleter = DefaultBoxPtrDeleter<T>, class... Args>
	inline Box<T, Deleter> makeBox(Args&&... args) {
		static_assert(
			std::is_default_constructible_v<Deleter>,
			"Deleter must be default constructible. This is not present as a requires "
		    "clause/concept usage, to prevent no-diagnostic-required cases on non matching "
		    "requirement friend redefinition."
		);
		return Box<T, Deleter>::fromPointer(new T(std::forward<Args>(args)...), Deleter{});
	}

	template<class T, class Deleter = DefaultBoxPtrDeleter<T>>
	using CBox = Box<const T, Deleter>;

	template<class T, class Deleter = DefaultBoxPtrDeleter<const T>>
	using MCBox = MBox<const T, Deleter>;
}

// global namespace export:
using base::Box;
using base::CBox;
using base::makeBox;
using base::MBox;
using base::MCBox;


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
