#pragma once

#include <memory>
#include "ref.hpp"


namespace base {

	template<class T>
	class Box final {
	private:
		std::unique_ptr<T> ptr;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("Box got nullptr");
			}
		}

	public:
		Box() = delete;

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
		Box(Box<U>&& other) noexcept: ptr(other.ptr) {
			other.ptr = nullptr;
		}

		Box& operator=(const Box& other) = delete;

		template<class U>
		Box& operator=(Box<U>&& oth) noexcept {
			ptr = oth.ptr;
			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(Box& first, Box& second) { std::swap(first.ptr, second.ptr); }

		Ref<T> refMut() const noexcept {
			return Ref<T>(ptr.get());
		}

		Ref<const T> ref() const noexcept {
			return Ref<T>(ptr.get());
		}

		T* operator->() const { assertNotNull(); return ptr; }

		T& operator*() const { assertNotNull(); return *ptr; }

		~Box() = default;
	};

	template<class T>
	class MBox final {
	private:
		// do we wan't to use unique here, or just make it over self?
		std::unique_ptr<T> ptr = nullptr;

		constexpr void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("MBox got nullptr");
			}
		}
	public:
		MBox() = default;

		/**
		 * @brief Constructs a Box from a raw pointer.
		 * @note It takes ownership of the pointer.
		 */
		explicit MBox(T* ptr) noexcept: ptr{ptr} { }

		MBox(const MBox& other) = delete;

		MBox(MBox&& other) noexcept: ptr{std::move(other.ptr)} {
			other.ptr = nullptr;
		}

		template<class U>
		MBox(MBox<U>&& other) noexcept: ptr(std::move(other.ptr)) {
			other.ptr = nullptr;
		}

		MBox& operator=(const MBox& other) = delete;

		template<class U>
		MBox& operator=(MBox<U>&& oth) noexcept {
			ptr = std::move(oth.ptr);
			oth.ptr = nullptr;
			return *this;
		}

		friend void swap(MBox& first, MBox& second) { std::swap(first.ptr, second.ptr); }

		MRef<T> refMut() const noexcept {
			return Ref<T>(ptr.get());
		}

		MRef<const T> ref() const noexcept {
			return Ref<T>(ptr.get());
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

	};

	template<class T, class... Args>
	inline Box<T> box(Args&&... args) {
		return Box<T>(new T(std::forward<Args>(args)...));
	}
}

// global namespace export:
using base::Box;
using base::box;


