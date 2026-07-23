#pragma once

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

namespace base {

	template<class T>
	class MRef;

	/**
	 * @brief A non-nullable pointer wrapper type, that does not own the pointer.
	 *
	 * @tparam T pointed type
	 */
	template<class T>
	class Ref final {
	private:
		T* ptr = nullptr;

		template<class U>
		friend class Ref;

		template<class U>
		friend class MRef;

		void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("Ref was in null state, when non-null was required!");
		}

	public:
		// Constructors from pointers:
		Ref() = delete;

		Ref(T* ptr): ptr{ ptr } { assertNotNull(); }

		Ref(std::nullptr_t) = delete;

		// @TODO: It would be preferred to assertNotNull during copy, but then the type is not
		// trivially copyable.

		// Copy:
		Ref(const Ref& other) noexcept = default;

		template<class U>
		Ref(const Ref<U>& other) noexcept: ptr{ other.get() } {}

		// @note: move constructors are not defined, since they are equivalent to copy constructors.
		// moving still works, because they are not deleted.

		// Assign:
		Ref& operator=(const Ref& other) noexcept = default;

		template<class U>
		Ref& operator=(const Ref<U>& other) noexcept {
			ptr = other.ptr;
			return *this;
		}

		// Acessors:

		[[nodiscard]]
		constexpr T* get() const noexcept {
			return ptr;
		}

		T& operator*() const { return *get(); }

		T* operator->() const noexcept { return get(); }

		MRef<T> toMRef() const noexcept { return MRef(ptr); }

		// Comparison:

		template<class U>
		bool operator==(const Ref<U>& other) const {
			return ptr == other.ptr;
		}

		template<class U>
		bool operator==(const MRef<U>& other) const {
			return ptr == other.ptr;
		}

		auto operator<=>(const Ref& other) const = default;

		// swap:
		friend void swap(Ref& first, Ref& second) noexcept { std::swap(first.ptr, second.ptr); }

		~Ref() = default;
	};

	/**
	 * @brief A nullable pointer wrapper type, that does not own the pointer.
	 * Implements both null-unchecked and null-checked access to the pointer.
	 * @note: When attempting to use a pointer when it is in null state, a panic will be thrown. In
	 * the future we might consider removing this check in release build for performance.
	 *
	 * @tparam T pointed type
	 */
	template<class T>
	class MRef final {
	private:
		T* ptr = nullptr;

		template<class U>
		friend class MRef;

		template<class U>
		friend class Ref;

		void assertNotNull() const {
			if (ptr == nullptr) CORE_PANIC("MRef was in null state, when non-null was required!");
		}

	public:
		// Constructors from pointers:
		constexpr MRef() = default;

		constexpr MRef(std::nullptr_t) {}

		MRef(T* ptr): ptr{ ptr } {}

		// Copy:
		MRef(const MRef& other) noexcept = default;

		template<class U>
		MRef(const MRef<U>& other) noexcept: ptr{ other.ptr } {}

		// @note: move constructors are not defined, since they are equivalent to copy constructors.
		// moving still works, because they are not deleted.

		// Construction from Ref:
		template<class U>
		MRef(const Ref<U>& other) noexcept: ptr{ other.get() } {}

		// Assign:
		MRef& operator=(std::nullptr_t) noexcept {
			ptr = nullptr;
			return *this;
		}

		MRef& operator=(const MRef& other) noexcept = default;

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

		/**
		 * @brief Null checked access method. Returns optional Ref to the pointed value.
		 * If MRef was in null state, the optional will be empty.
		 * If MRef was not in null state, the optional will contain Ref to the pointed value.
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
		 * @note panics if MRef was in null state.
		 */
		T* operator->() const {
			assertNotNull();
			return ptr;
		}

		/**
		 * @brief Null unchecked access method. Works like "*" operator on typical pointer.
		 * @note panics if MRef was in null state.
		 */
		T& operator*() const {
			assertNotNull();
			return *ptr;
		}

		// Comparison:

		bool operator==(std::nullptr_t) const { return ptr == nullptr; }

		template<class U>
		bool operator==(const MRef<U>& other) const {
			return ptr == other.ptr;
		}

		template<class U>
		bool operator==(const Ref<U>& other) const {
			return ptr == other.ptr;
		}

		// swap:
		friend void swap(MRef& first, MRef& second) noexcept { std::swap(first.ptr, second.ptr); }

		operator bool() const { return ptr; }

		~MRef() = default;
	};

	// Deduction guide for constructing a MRef from a Ref:
	template<class U>
	MRef(const Ref<U>&) -> MRef<U>;

	template<class T>
	using CRef = Ref<const T>;

	template<class T>
	using MCRef = MRef<const T>;


/**
 * @brief Macro used to expose ref access in ref-like objects that manage a ref internally.
 */
#define EXPOSE_REF_INTERFACE(element_name)                         \
	auto  operator->() const { return element_name.operator->(); } \
	auto& operator*() const { return element_name.operator*(); }

}

// global namespace export:
using base::CRef;
using base::MCRef;
using base::MRef;
using base::Ref;
