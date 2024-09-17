#pragma once

#include "ref.hpp"

namespace base {

	/**
	 * @brief A pointer wrapper type, that owns the pointer and deletes it when it goes out of
	 * scope. It is not nullable, and it is not copyable.
	 * @note: When performing a move operation, the source pointer is set to nullptr.
	 * Attempt to use it after that will result in a panic. In the future we might consider
	 * removing this check in release build for performance.
	 *
	 * @tparam T pointed type
	 */
	template<class T>
	class Box final {
	private:
		T* ptr;

		template<class U>
		friend class Box;

		template<class U>
		friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) RIFT_PANIC("Box was in null state, when non-null was required!");
		}

	public:
		Box()               = delete;
		Box(std::nullptr_t) = delete;

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit Box(T* ptr) noexcept: ptr{ ptr } { assertNotNull(); }

		Box(const Box& other) = delete;

		Box(Box&& other) noexcept: ptr{ std::move(other).ptr } { other.ptr = nullptr; }

		template<class U>
		Box(Box<U>&& other) noexcept: ptr{ std::move(other).ptr } {
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
		Box& operator=(Box<U>&& oth) noexcept {
			delete ptr;
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

		~Box() { delete ptr; }
	};

	/**
	 * @brief A nullable pointer wrapper type, that owns the pointer and deletes it when it goes out
	 * of scope. Implements both null-unchecked and null-checked access to the pointer. It is not
	 * copyable.
	 * @note: When attempting to use a pointer when it is in null state, a panic will be thrown. In
	 * the future we might consider removing this check in release build for performance.
	 *
	 * @tparam T pointed type
	 */
	template<class T>
	class MBox final {
	private:
		T* ptr = nullptr;

		template<class U>
		friend class MBox;

		template<class U>
		friend class Box;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) RIFT_PANIC("MBox was in null state, when non-null was required!");
		}

	public:
		MBox() = default;
		MBox(std::nullptr_t){};

		/**
		 * @brief Constructs an MBox from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit MBox(T* ptr) noexcept: ptr{ ptr } {}

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept: ptr{ std::move(other).ptr } { other.ptr = nullptr; }

		template<class U>
		MBox(Box<U>&& other) noexcept: ptr{ std::move(other).ptr } {
			other.ptr = nullptr;
		}

		template<class U>
		MBox(MBox<U>&& other) noexcept: ptr{ std::move(other).ptr } {
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
		MBox& operator=(MBox<U>&& oth) noexcept {
			delete ptr;
			ptr     = std::move(oth).ptr;
			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(MBox& first, MBox& second) noexcept { std::swap(first.ptr, second.ptr); }

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
		 * @return Optional<Ref<T>>
		 */
		Optional<Box<T>> toOptBox() && {
			T* output = ptr;
			ptr       = nullptr;
			if (output == nullptr) return {};
			return Box<T>(output);
		}

		~MBox() { delete ptr; }
	};

	// Deduction guide for constructing a MBox from a Box:
	template<class U>
	MBox(Box<U>&&) noexcept -> MBox<U>;

	template<class T, class... Args>
	inline Box<T> makeBox(Args&&... args) {
		return Box<T>(new T(std::forward<Args>(args)...));
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
