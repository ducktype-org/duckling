#pragma once

#include "ref.hpp"


namespace base {

	template<class T>
	class Box final {
	private:
		T* ptr;

		template<class U>
		friend class Box;

		template<class U>
		friend class MBox;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("Box got nullptr");
			}
		}

	public:
		Box() = delete;
		Box(std::nullptr_t) = delete;

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit Box(T* ptr) noexcept: ptr{ptr} { assertNotNull(); }

		Box(const Box& other) = delete;

		// @TODO: do we make Box secretly nullable to enforce move semantics or do we not?

		Box(Box&& other) noexcept: ptr{std::move(other.ptr)} {
			other.ptr = nullptr;
		}

		template<class U>
		Box(Box<U>&& other) noexcept: ptr(std::move(other).ptr) {
			other.ptr = nullptr;
		}

		Box& operator=(const Box& other) = delete;

		// @TODO: do we need move operations here? Maybe copy will by enough?
		// do we want this = ?
		template<class U>
		Box& operator=(Box<U>&& oth) noexcept {
			delete ptr;
			ptr = std::move(oth).ptr;
			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(Box& first, Box& second) noexcept { std::swap(first.ptr, second.ptr); }

		Ref<T> refMut() const noexcept {
			return Ref<T>(ptr);
		}

		Ref<const T> ref() const noexcept {
			return Ref<const T>(ptr);
		}

		T* operator->() const { assertNotNull(); return ptr; }

		T& operator*() const { assertNotNull(); return *ptr; }

		~Box() {
			delete ptr;
			ptr = nullptr;
		};
	};

	template<class T>
	class MBox final {
	private:
		// do we wan't to use unique here, or just make it over self?
		T* ptr = nullptr;

		template<class U>
		friend class MBox;

		template<class U>
		friend class Box;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("MBox got nullptr");
			}
		}
	public:
		MBox() = default;
		MBox(std::nullptr_t) {};

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit MBox(T* ptr) noexcept: ptr{ptr} { }

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept: ptr{std::move(other).ptr} {
			other.ptr = nullptr;
		}

		template<class U>
		MBox(Box<U>&& other) noexcept: ptr{std::move(other).ptr} {
			other.ptr = nullptr;
		}

		template<class U>
		MBox(MBox<U>&& other) noexcept: ptr(std::move(other).ptr) {
			other.ptr = nullptr;
		}

		MBox& operator=(const MBox& other) = delete;

		// do we want it?
		template<class U>
		MBox& operator=(MBox<U>&& oth) noexcept {
			delete ptr;
			ptr = std::move(oth).ptr;
			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(MBox& first, MBox& second) noexcept { std::swap(first.ptr, second.ptr); }

		MRef<T> refMut() const noexcept {
			return MRef<T>(ptr);
		}

		MRef<const T> ref() const noexcept {
			return MRef<T>(ptr);
		}

		[[nodiscard]]
		constexpr Optional<Ref<T>> get() const noexcept {
			if (ptr == nullptr) {
				return {};
			}
			return Ref<T>(ptr);
		}

		// unsafe access:
		T* operator->() const { assertNotNull(); return ptr; }
		T& operator*() const { assertNotNull(); return *ptr; }

		Box<T> stealBox() && {
			assertNotNull();
			T* output = ptr;
			ptr = nullptr;
			return Box<T>(output);
		}

		~MBox() {
			delete ptr;
		};
	};

	// Deduction guide for constructing a MBox from a Box:
	template<class U>
	MBox(Box<U>&&) noexcept -> MBox<U>;

	template<class T, class... Args>
	inline Box<T> box(Args&&... args) {
		return Box<T>(new T(std::forward<Args>(args)...));
	}

	template<class T>
	using CBox = Box<const T>;

	template<class T>
	using MCBox = MBox<const T>;
}

// global namespace export:
using base::Box;
using base::MBox;
using base::CBox;
using base::MCBox;
using base::box;
