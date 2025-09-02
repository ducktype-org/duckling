#pragma once

#include "is_complete.hpp"
#include "ref.hpp"

namespace base {

	namespace extend {
		/**
		 * @brief Pointer deleter functor used by Box, MBox.
		 * @note Adding specialization for custom types
		 * can be used to avoid delete on incomplete types.
		 *
		 * @tparam T
		 */
		template<class T>
		struct BoxPtrDeleter {
			static_assert(IS_COMPLETE_V<T>);

			static void del(T* ptr) { delete ptr; }
		};
	}

	/**
	 * @brief Default deleter for Box/MBox that uses the legacy BoxPtrDeleter system.
	 * This maintains backward compatibility with existing specializations.
	 * 
	 * @tparam T The type to delete
	 */
	template<class T>
	struct DefaultDeleter {
		void operator()(T* ptr) const {
			extend::BoxPtrDeleter<T>::del(ptr);
		}
	};

	/**
	 * @brief Function pointer deleter for Box/MBox.
	 * Useful for C-style APIs or custom deletion functions.
	 * 
	 * @tparam T The type to delete
	 */
	template<class T>
	struct FunctionDeleter {
		using DeleterFunction = void(*)(T*);
		DeleterFunction deleter;

		explicit FunctionDeleter(DeleterFunction del) : deleter(del) {}

		void operator()(T* ptr) const {
			if (deleter && ptr) {
				deleter(ptr);
			}
		}
	};

	/**
	 * @brief Macro to create a simple function-based deleter.
	 * Usage: DUCKLING_MAKE_DELETER(MyDeleter, my_delete_function)
	 * This creates a deleter type that calls the specified function.
	 */
	#define DUCKLING_MAKE_DELETER(DeleterName, DeleteFunction) \
		struct DeleterName { \
			template<class T> \
			void operator()(T* ptr) const { \
				DeleteFunction(ptr); \
			} \
		}

	/**
	 * @brief Macro to create a method-based deleter.
	 * Usage: DUCKLING_MAKE_METHOD_DELETER(MyDeleter, destroy)
	 * This creates a deleter type that calls the specified method on the object.
	 */
	#define DUCKLING_MAKE_METHOD_DELETER(DeleterName, MethodName) \
		struct DeleterName { \
			template<class T> \
			void operator()(T* ptr) const { \
				if (ptr) { \
					ptr->MethodName(); \
					delete ptr; \
				} \
			} \
		}

	/**
	 * @brief A pointer wrapper type, that owns the pointer and deletes it when it goes out of
	 * scope. It is not nullable, and it is not copyable.
	 * @note: When performing a move operation, the source pointer is set to nullptr.
	 * Attempt to use it after that will result in a panic. In the future we might consider
	 * removing this check in release build for performance.
	 *
	 * @tparam T pointed type
	 * @tparam Deleter deleter type, defaults to DefaultDeleter<T> for backward compatibility
	 */
	template<class T, class Deleter = DefaultDeleter<T>>
	class Box final {
	private:
		T* ptr;
		[[no_unique_address]] Deleter deleter;

		template<class U, class UDeleter>
		friend class Box;

		template<class U, class UDeleter>
		friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("Box was in null state, when non-null was required!");
		}

		template<class UDeleter>
		Deleter initializeDeleter(UDeleter&& other_deleter) {
			if constexpr (std::is_same_v<Deleter, std::remove_cvref_t<UDeleter>>) {
				return std::forward<UDeleter>(other_deleter);
			} else {
				return Deleter{};
			}
		}

		explicit Box(T* ptr) noexcept: ptr{ ptr }, deleter{} { assertNotNull(); }

		explicit Box(T* ptr, const Deleter& del) noexcept: ptr{ ptr }, deleter{ del } { assertNotNull(); }

		explicit Box(T* ptr, Deleter&& del) noexcept: ptr{ ptr }, deleter{ std::move(del) } { assertNotNull(); }

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

		/**
		 * @brief Constructs a Box from a raw pointer with a custom deleter.
		 * It takes ownership of the pointer.
		 */
		static Box fromPointer(T* ptr, const Deleter& del) noexcept { return Box(ptr, del); }

		/**
		 * @brief Constructs a Box from a raw pointer with a custom deleter (move version).
		 * It takes ownership of the pointer.
		 */
		static Box fromPointer(T* ptr, Deleter&& del) noexcept { return Box(ptr, std::move(del)); }

		Box(const Box& other) = delete;

		Box(Box&& other) noexcept: ptr{ std::move(other).ptr }, deleter{ std::move(other.deleter) } { other.ptr = nullptr; }

		template<class U, class UDeleter>
		Box(Box<U, UDeleter>&& other) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>))
			: ptr{ std::move(other).ptr }, deleter{ initializeDeleter<UDeleter>(std::move(other.deleter)) } {
			other.ptr = nullptr;
		}

		Box& operator=(const Box& other) = delete;

		/**
		 * @brief Move assignment. The object previously pointed to by the Box is deleted.
		 *
		 * @tparam U
		 * @tparam UDeleter
		 * @param oth
		 * @return Box&
		 */
		template<class U, class UDeleter>
		Box& operator=(Box<U, UDeleter>&& oth) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>)) {
			deleter(ptr);
			ptr     = std::move(oth).ptr;
			if constexpr (std::is_same_v<Deleter, UDeleter>) {
				deleter = std::move(oth.deleter);
			} else {
				deleter = Deleter{};
			}
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

		~Box() {
			deleter(ptr);
		}
	};

	/**
	 * @brief A nullable pointer wrapper type, that owns the pointer and deletes it when it goes out
	 * of scope. Implements both null-unchecked and null-checked access to the pointer. It is not
	 * copyable.
	 * @note: When attempting to use a pointer when it is in null state, a panic will be thrown. In
	 * the future we might consider removing this check in release build for performance.
	 *
	 * @tparam T pointed type
	 * @tparam Deleter deleter type, defaults to DefaultDeleter<T> for backward compatibility
	 */
	template<class T, class Deleter = DefaultDeleter<T>>
	class MBox final {
	private:
		T* ptr = nullptr;
		[[no_unique_address]] Deleter deleter;

		template<class U, class UDeleter>
		friend class MBox;

		template<class U, class UDeleter>
		friend class Box;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("MBox was in null state, when non-null was required!");
		}

		template<class UDeleter>
		Deleter initializeDeleter(UDeleter&& other_deleter) {
			if constexpr (std::is_same_v<Deleter, std::remove_cvref_t<UDeleter>>) {
				return std::forward<UDeleter>(other_deleter);
			} else {
				return Deleter{};
			}
		}

		/**
		 * @brief Constructs an MBox from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit MBox(T* ptr) noexcept: ptr{ ptr }, deleter{} {}

		explicit MBox(T* ptr, const Deleter& del) noexcept: ptr{ ptr }, deleter{ del } {}

		explicit MBox(T* ptr, Deleter&& del) noexcept: ptr{ ptr }, deleter{ std::move(del) } {}

	public:
		MBox() = default;

		MBox(std::nullptr_t) : deleter{} {}

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept: ptr{ std::move(other).ptr }, deleter{ std::move(other.deleter) } { other.ptr = nullptr; }

		template<class U, class UDeleter>
		MBox(Box<U, UDeleter>&& other) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>))
			: ptr{ std::move(other).ptr }, deleter{ initializeDeleter<UDeleter>(std::move(other.deleter)) } {
			other.ptr = nullptr;
		}

		template<class U, class UDeleter>
		MBox(MBox<U, UDeleter>&& other) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>))
			: ptr{ std::move(other).ptr }, deleter{ initializeDeleter<UDeleter>(std::move(other.deleter)) } {
			other.ptr = nullptr;
		}

		MBox& operator=(const MBox& other) = delete;

		/**
		 * @brief Move assignment. The object previously pointed to by the MBox is deleted.
		 *
		 * @tparam U
		 * @tparam UDeleter
		 * @param oth
		 * @return MBox&
		 */
		template<class U, class UDeleter>
		MBox& operator=(MBox<U, UDeleter>&& oth) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>)) {
			deleter(ptr);
			ptr     = std::move(oth).ptr;
			if constexpr (std::is_same_v<Deleter, UDeleter>) {
				deleter = std::move(oth.deleter);
			} else {
				deleter = Deleter{};
			}
			oth.ptr = nullptr;
			return *this;
		}

		/**
		 * @brief Move assignment. The object previously pointed to by the MBox is deleted.
		 *
		 * @tparam U
		 * @tparam UDeleter
		 * @param oth
		 * @return MBox&
		 */
		template<class U, class UDeleter>
		MBox& operator=(Box<U, UDeleter>&& oth) noexcept requires(std::is_convertible_v<U*, T*> && (std::is_same_v<Deleter, UDeleter> || std::is_default_constructible_v<Deleter>)) {
			deleter(ptr);
			ptr     = std::move(oth).ptr;
			if constexpr (std::is_same_v<Deleter, UDeleter>) {
				deleter = std::move(oth.deleter);
			} else {
				deleter = Deleter{};
			}
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
		 * @return Optional<Box<T, Deleter>>
		 */
		Optional<Box<T, Deleter>> toOptBox() && {
			T* output = ptr;
			ptr       = nullptr;
			if (output == nullptr) return {};
			return Box<T, Deleter>::fromPointer(output, std::move(deleter));
		}

		~MBox() { deleter(ptr); }
	};

	// Deduction guide for constructing a MBox from a Box:
	template<class U, class UDeleter>
	MBox(Box<U, UDeleter>&&) noexcept -> MBox<U, UDeleter>;

	template<class T, class... Args>
	inline Box<T> makeBox(Args&&... args) {
		return Box<T>::fromPointer(new T(std::forward<Args>(args)...));
	}

	template<class T>
	using CBox = Box<const T>;

	template<class T>
	using MCBox = MBox<const T>;
}

// global namespace export:
using base::Box;
using base::CBox;
using base::makeBox;
using base::MBox;
using base::MCBox;
