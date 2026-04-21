#pragma once
#include "box.hpp"
#include "ref.hpp"

#include <base/except/exceptions.hpp>

#include <variant>

namespace base {
	/**
	 * @brief Type that holds either a Box<T> or a CRef<T>, providing a unified interface to access
	 * the underlying T.
	 * @tparam T The type of the object being held.
	 */
	template<typename T>
	class BoxOrCRef final {
		using StorageType = std::variant<Box<T>, CRef<T>>;
		StorageType storage;

	public:
		template<typename U>
		requires std::is_base_of_v<T, U> BoxOrCRef(Box<U> box): storage(Box<T>(std::move(box))) {}

		template<typename U>
		requires std::is_base_of_v<T, U> BoxOrCRef(CRef<U> ref): storage(CRef<T>(ref)) {}

		template<typename U>  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires std::is_base_of_v<T, U> BoxOrCRef(BoxOrCRef<U>&& other) noexcept:
			  storage(
				  other.isBox() ? StorageType(Box<T>(std::move(other.getBox())))
								: StorageType(other.getRef())
			  ) {}

		template<typename U>  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires std::is_base_of_v<T, U> BoxOrCRef& operator=(BoxOrCRef<U>&& other) noexcept {
			if (other.isBox())
				storage = Box<T>(std::move(other.getBox()));
			else
				storage = other.getRef();
			return *this;
		}

		BoxOrCRef(const BoxOrCRef&)            = delete;
		BoxOrCRef& operator=(const BoxOrCRef&) = delete;


		BoxOrCRef(BoxOrCRef&&)            = default;
		BoxOrCRef& operator=(BoxOrCRef&&) = default;

		[[nodiscard]]
		bool isBox() const {
			return std::holds_alternative<Box<T>>(storage);
		}

		[[nodiscard]]
		bool isRef() const {
			return std::holds_alternative<CRef<T>>(storage);
		}

		[[nodiscard]]
		Box<T>& getBox() {
			if (not isBox()) CORE_PANIC("Called getBox on BoxOrCRef that holds Ref");
			return std::get<Box<T>>(storage);
		}

		[[nodiscard]]
		const Box<T>& getBox() const {
			if (not isBox()) CORE_PANIC("Called getBox on BoxOrCRef that holds Ref");
			return std::get<Box<T>>(storage);
		}

		[[nodiscard]]
		const CRef<T>& getRef() const {
			if (not isRef()) CORE_PANIC("Called getRef on BoxOrCRef that holds Box");
			return std::get<CRef<T>>(storage);
		}

		CRef<T> ref() const {
			if (isBox())
				return getBox().ref();
			else
				return getRef();
		}

		const T* operator->() const { return ref().get(); }

		const T& operator*() const { return *ref(); }

		const T* get() const { return ref().get(); }

		bool operator==(const BoxOrCRef& other) const { return get() == other.get(); }
	};
}

using base::BoxOrCRef;
