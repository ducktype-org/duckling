#pragma once

#include "is_complete.hpp"
#include "ref.hpp"

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
	struct DefaultBoxPtrDeleter {
		static_assert(IS_COMPLETE_V<T>);

		static void del(T* ptr) { delete ptr; }
	};


	/**
	 * @brief A pointer wrapper type, that owns the pointer and deletes it when it goes out of
	 * scope. It is not nullable, and it is not copyable.
	 * @note: When performing a move operation, the source pointer is set to nullptr.
	 * Attempt to use it after that will result in a panic. In the future we might consider
	 * removing this check in release build for performance.
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
	class Box final {
	private:
		T* ptr;

		template<class U, class UDeleter>
		friend class Box;

		template<class U, class UDeleter>
		friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("Box was in null state, when non-null was required!");
		}

		explicit Box(T* ptr) noexcept: ptr{ ptr } { assertNotNull(); }

		/*
		 * Shorthand for creating a Box with the same deleter.
		 */
		template<class U>
		using SDBox = Box<U, Deleter>;

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
		static Box fromPointer(T* ptr) noexcept { return Box(ptr); }

		Box(const Box& other) = delete;

		Box(Box&& other) noexcept: ptr{ std::move(other).ptr } { other.ptr = nullptr; }

		template<class U>
		Box(SDBox<U>&& other) noexcept: ptr{ std::move(other).ptr } {
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
		template<class U>
		Box& operator=(SDBox<U>&& oth) noexcept {
			Deleter::del(ptr);
			ptr     = std::move(oth).ptr;
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

		~Box() { Deleter::del(ptr); }
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
		T* ptr = nullptr;

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
		explicit MBox(T* ptr) noexcept: ptr{ ptr } {}

		/**
		 * @brief Shorthand for creating a Box with the same deleter.
		 */
		template<class U>
		using SDBox = Box<U, Deleter>;

		/**
		 * @brief Shorthand for creating a MBox with the same deleter.
		 */
		template<class U>
		using SDMBox = MBox<U, Deleter>;

	public:
		MBox() = default;

		MBox(std::nullptr_t) {}

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept: ptr{ std::move(other).ptr } { other.ptr = nullptr; }

		template<class U>
		MBox(SDBox<U>&& other) noexcept: ptr{ std::move(other).ptr } {
			other.ptr = nullptr;
		}

		template<class U>
		MBox(SDMBox<U>&& other) noexcept: ptr{ std::move(other).ptr } {
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
		template<class U>
		MBox& operator=(SDMBox<U>&& oth) noexcept {
			Deleter::del(ptr);
			ptr     = std::move(oth).ptr;
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
		template<class U>
		MBox& operator=(SDBox<U>&& oth) noexcept {
			Deleter::del(ptr);
			ptr     = std::move(oth).ptr;
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
		Optional<SDBox<T>> toOptBox() && {
			T* output = ptr;
			ptr       = nullptr;
			if (output == nullptr) return {};
			return SDBox<T>::fromPointer(output);
		}

		~MBox() { Deleter::del(ptr); }
	};

	// Deduction guide for constructing a MBox from a Box:
	template<class U, class UDeleter>
	MBox(Box<U, UDeleter>&&) noexcept -> MBox<U, UDeleter>;

	template<class T, class Deleter = DefaultBoxPtrDeleter<T>, class... Args>
	inline Box<T, Deleter> makeBox(Args&&... args) {
		return Box<T, Deleter>::fromPointer(new T(std::forward<Args>(args)...));
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
#define DEFAULT_BOX_PTR_DELETER_DECLARATION(T) \
	template<>                                 \
	struct ::base::DefaultBoxPtrDeleter<T> {   \
		static void del(T* ptr);               \
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
#define DEFAULT_BOX_PTR_DELETER_DEFINITION(T) \
	void ::base::DefaultBoxPtrDeleter<T>::del(T* ptr) { delete ptr; }
