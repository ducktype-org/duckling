#pragma once

#include "borrow_pointer.hpp"
#include <memory>

namespace base {
	template<class T>
	class unique_ptr: public std::unique_ptr<T> {
	public:
		constexpr unique_ptr() noexcept: std::unique_ptr<T>() {}

		explicit unique_ptr(T *ptr) noexcept: std::unique_ptr<T>(ptr) {}

		constexpr unique_ptr(std::nullptr_t) noexcept: std::unique_ptr<T>(nullptr) {}

		unique_ptr(const unique_ptr<T> &) = delete;

		template<class U>
		unique_ptr(unique_ptr<U> &&other) noexcept: unique_ptr() {
			this->reset(other.release());
		}

		unique_ptr &operator=(std::nullptr_t) noexcept {
			this->reset(nullptr);
			return *this;
		}

		template<class U>
		unique_ptr &operator=(unique_ptr<U> &&oth) noexcept {
			this->reset(oth.release());
			return *this;
		}

		friend void swap(unique_ptr<T> &first, unique_ptr<T> &second) { first.swap(second); }

		borrow_ptr<T> borrow_mut() noexcept { return borrow_ptr<T>(this->get()); }

		c_borrow_ptr<T> borrow() const noexcept { return c_borrow_ptr<T>(this->get()); }
	};

	template<class T, class... Args>
	inline unique_ptr<T> make_unique(Args &&...args) {
		return unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

}
