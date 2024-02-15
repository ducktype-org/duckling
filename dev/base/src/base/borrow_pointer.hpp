#pragma once

#include <utility>

namespace base {
	template<class T>
	class unique_ptr;

	/**
	 * @brief Borrow pointer observing a value of type `T` that is managed by a `base::unique_ptr`.
	 */
	template<class T>
	class borrow_ptr {
	private:
		T* ptr;

		template<class U>
		friend class borrow_ptr;

	public:
		constexpr borrow_ptr() noexcept: ptr(nullptr) {}

		explicit borrow_ptr(T* ptr) noexcept { this->ptr = ptr; }

		constexpr borrow_ptr(std::nullptr_t) noexcept: ptr(nullptr) {}

		template<class U>
		bool operator==(const borrow_ptr<U>& oth) const {
			return ptr == oth.get();
		}

		bool operator==(std::nullptr_t) const { return ptr == nullptr; }

		borrow_ptr(const borrow_ptr<T>& other) noexcept = default;

		borrow_ptr(borrow_ptr<T>&& other) noexcept: borrow_ptr() { swap(*this, other); }

		template<class U>
		borrow_ptr(const borrow_ptr<U>& other) noexcept: ptr(static_cast<T*>(other.ptr)) {}

		friend void swap(borrow_ptr<T>& first, borrow_ptr<T>& second) noexcept {
			std::swap(first.ptr, second.ptr);
		}

		borrow_ptr<T>& operator=(const borrow_ptr<T>& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		~borrow_ptr() = default;

		T& operator*() const { return *get(); }

		T* operator->() const noexcept { return get(); }

		T* get() const noexcept { return ptr; }
	};

	/**
	 * @brief Immutable version of `base::borrow_ptr<T>` that can't change the underlying value.
	 */
	template<class T>
	using c_borrow_ptr = borrow_ptr<const T>;

}
