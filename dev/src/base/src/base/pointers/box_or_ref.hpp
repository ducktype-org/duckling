#pragma once
#include "box.hpp"
#include "ref.hpp"

#include "base/except/exceptions.hpp"

#include <variant>

namespace base {
	template<typename T>
	class BoxOrCRef final {
		using StorageType = std::variant<Box<T>, Ref<const T>>;
		StorageType storage;

	public:
		template<typename U>
		requires std::is_base_of_v<T, U> BoxOrCRef(Box<U> box): storage(Box<T>(std::move(box))) {}

		template<typename U>
		requires std::is_base_of_v<T, U> BoxOrCRef(Ref<const U> ref): storage(Ref<const T>(ref)) {}

		template<typename U>  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires std::is_base_of_v<T, U> BoxOrCRef(BoxOrCRef<U>&& other):
			  storage(
				  other.isBox() ? StorageType(Box<T>(std::move(other.getBox())))
								: StorageType(other.getRef())
			  ) {}

		template<typename U>  // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		requires std::is_base_of_v<T, U> BoxOrCRef& operator=(BoxOrCRef<U>&& other) {
			if (other.isBox())
				storage = Box<T>(std::move(other.getBox()));
			else
				storage = other.getRef();
			return *this;
		}

		[[nodiscard]]
		bool isBox() const {
			return std::holds_alternative<Box<T>>(storage);
		}

		[[nodiscard]]
		bool isRef() const {
			return std::holds_alternative<Ref<const T>>(storage);
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
		const Ref<const T>& getRef() const {
			if (not isRef()) CORE_PANIC("Called getRef on BoxOrCRef that holds Box");
			return std::get<Ref<const T>>(storage);
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

		bool operator==(const BoxOrCRef& other) const {
			if (isBox() && other.isBox())
				return getBox() == other.getBox();
			else if (isRef() && other.isRef())
				return getRef() == other.getRef();
			else
				return false;
		}
	};
}

using base::BoxOrCRef;
