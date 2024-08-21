#pragma once

#include "exceptions.hpp"
#include "optional.hpp"

namespace base {

	/**
	 * @brief Reference
	 */
	template<class T>
	class Ref final {
	private:
		T* ptr;

		template<class U>
		friend class Ref;

		void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("Ref got nullptr");
			}
		}
	public:

		// Constructors from pointers:
		Ref() = delete;
		Ref(T* ptr): ptr{ptr} { assertNotNull(); }
		
		// Copy:
		Ref(const Ref& other) noexcept: ptr(other.get()) {}
		
		template<class U>
		Ref(const Ref<U>& other) noexcept: ptr(other.get()) {}

		// Move:
		Ref(Ref&& other) noexcept: ptr{other.ptr} { }

		template<class U>
		Ref(Ref<U>&& other) noexcept: ptr(other.ptr) {}

		// Assign:
		Ref& operator=(const Ref& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		template<class U>
		Ref& operator=(const Ref<U>& other) noexcept {
			ptr = other.ptr;
			return *this;
		}


		// Acessors:

		[[nodiscard]]
		constexpr T* get() const noexcept { return ptr; }

		T& operator*() const { return *get(); }

		T* operator->() const noexcept { return get(); }

		// Comparison:

		template<class U>
		bool operator==(const Ref<U>& other) const {
			return ptr == other.get();
		}

		// swap:
		friend void swap(Ref& first, Ref& second) noexcept {
			std::swap(first.ptr, second.ptr);
		}

		~Ref() = default;
	};

	/**
	 * @brief Maybe reference
	 */
	template<class T>
	class MRef final {
	private:
		T* ptr = nullptr;

		template<class U>
		friend class MRef;

		void assertNotNull() const {
			if (ptr == nullptr) {
				RIFT_PANIC("MRef got nullptr");
			}
		}
	public:
		// Constructors from pointers:
		MRef() = default;
		MRef(std::nullptr_t) = default;

		MRef(T* ptr): ptr{ptr} { }

		// Copy:
		MRef(const MRef& other) noexcept: ptr(other.get()) {}
		
		template<class U>
		MRef(const MRef<U>& other) noexcept: ptr(other.get()) {}

		// Move:
		MRef(MRef&& other) noexcept: ptr{other.ptr} { }

		template<class U>
		MRef(MRef<U>&& other) noexcept: ptr(other.ptr) {}

		// Construction from Ref:
		template<class U>
		MRef(const Ref<U>& other) noexcept: ptr(other.get()) {}

		// Assign:
		MRef& operator=(std::nullptr_t) noexcept {
			ptr= nullptr;
			return *this;
		}

		MRef& operator=(const MRef& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		template<class U>
		MRef& operator=(const MRef<U>& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		template<class U>
		MRef& operator=(const Ref<U>& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		// Acessors:

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

		// Comparison:

		template<class U>
		bool operator==(const MRef<U>& other) const {
			return ptr == other.ptr;
		}

		// swap:
		friend void swap(MRef& first, MRef& second) noexcept {
			std::swap(first.ptr, second.ptr);
		}

		~MRef() = default;
	};
}

// global namespace export:
using base::Ref;
