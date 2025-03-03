#pragma once

#include <base/box.hpp>
#include <base/ref.hpp>

namespace pst {
	/**
	 * @brief Some kind of id in the future
	 */
	class PSTAccessKey final {
	private:
		int key = 0;

	public:
		PSTAccessKey()                    = default;
		PSTAccessKey(const PSTAccessKey&) = default;

		bool operator==(PSTAccessKey other) { return key == other.key; }
	};

	template</*std::derived_from<LangElement>*/ typename Element>
	class Access final {
	private:
		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessLocked;
		template</*std::derived_from<LangElement>*/ typename E>
		friend class Access;

 		MCRef<Element> ref;
		PSTAccessKey stored_key;

		Access(MCRef<Element> ref, PSTAccessKey key): ref(ref), stored_key(key) {}
	public:
		Access()               = delete;
		Access(const Access&)        = delete;
		Access(Access&& other) = default;

		template<typename E>
		Access& operator=(Access<E>&& oth) noexcept {
			ref = std::move(oth.ref);
			stored_key = oth.stored_key;
			return *this;
		}

		template<typename T>
		Access(Access<T>&& other): ref(std::move(other.ref)), stored_key(other.stored_key) {}

		EXPOSE_MREF_INTERFACE(ref)
	};

	template</*std::derived_from<LangElement>*/ typename Element>
	class AccessLocked {
	private:
		MCRef<Element> ref;

		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessInternal;
		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessLocked;

		AccessLocked(MCRef<Element> ref): ref(ref) {}

	public:
		AccessLocked()               = delete;
		AccessLocked(AccessLocked&)  = default;
		AccessLocked(AccessLocked&&) = default;

		template<typename T>
		AccessLocked(AccessLocked<T> other): ref(other.ref) {}

		template<typename T>
		AccessLocked(AccessLocked<T>& other): ref(other.ref) {}

		template<std::derived_from<Element> Desc>
		AccessLocked<Desc> cast() const {
			return { dynamic_cast<const Desc*>(&*ref) };
		}

		Access<Element> unlock(PSTAccessKey key) const { return { ref, key }; }
	};

	template</*std::derived_from<LangElement>*/ typename Element>
	class AccessInternal final{
	private:
		MBox<Element> box;		

		template<typename E>
		friend class AccessInternal;

	public:
		AccessInternal() = default;
		AccessInternal(const AccessInternal<Element>&) = default;
		AccessInternal(AccessInternal<Element>&&) = default;
		AccessInternal(MBox<Element>&& box): box(std::move(box)) {}

		AccessLocked<Element> give() const { return { box.ref() }; }

		template<typename E>
		AccessInternal& operator=(AccessInternal<E>&& oth) noexcept {
			box = std::move(oth.box);
			return *this;
		}

		template<typename E>
		AccessInternal& operator=(MBox<E>&& box_) noexcept {
			box = std::move(box_);
			return *this;
		}

		/**
		 * @brief For internal usage of an Element, the alternative is to make Element a friend of
		 * AccessInternal but it allows for easier access leaks.
		 */
		MCRef<Element> internal() const { return box.ref(); }
	};
}