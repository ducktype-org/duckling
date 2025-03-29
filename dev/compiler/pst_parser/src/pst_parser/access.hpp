#pragma once

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/optional.hpp>

#include <query_framework/query_int.hpp>

#include "lang_parser_element.hpp"

namespace pst {
	/**
	 * @brief Wrapper for a reference to pst that allows access, it should never be passed between
	 * different queries.
	 */
	template</*std::derived_from<LangElement>*/ typename Element>
	class Access final {
	private:
		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessLocked;
		template</*std::derived_from<LangElement>*/ typename E>
		friend class Access;
		friend Element;

		CRef<Element> ref;

		Access(CRef<Element> ref): ref(ref) {}

		Access(const Element& ref): ref(&ref) {}

	public:
		Access()               = delete;
		Access(const Access&)  = default;
		Access(Access&& other) = default;

		template<typename E>
		Access& operator=(Access<E>&& oth) noexcept {
			ref = std::move(oth).ref;
			return *this;
		}

		template<typename T>
		Access(Access<T>&& other): ref(std::move(other).ref) {}

		/**
		 * @brief This should be fine for now, casting might end up as null which would be
		 * potentially bad for knowing about accesses
		 */
		template<typename T>
		base::Optional<Access<T>> dynamicCast() const {
			if (auto ptr = dynamic_cast<const T*>(&*ref))
				return { { ptr } };
			else
				return {};
		}

		EXPOSE_REF_INTERFACE(ref)
	};

	namespace detail {
		/**
		 * @brief Notification to context About the access to an element.
		 * @note For now we keep PstID as the key, but in the future it will likely be changed to hash based on hash-framework.
		 */
		void notifyContext(query::Context& ctx, PstID id);
		/**
		 * @brief Some smart throw about bad access(unsafe access of non-null) based on context.
		 */
		void notifyBadAccess(query::Context&);
	}

	/**
	 * @brief Wrapper for an optional reference to pst that doesn't allow for normal access without
	 * passing context.
	 *
	 * @note The access is still allowed with illegalAccess but it should only be used outside of
	 * the normal query framework unless for debuging access.
	 */
	template</*std::derived_from<LangElement>*/ typename Element>
	class AccessLocked final {
	private:
		MCRef<Element> ref;

		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessInternal;
		template</*std::derived_from<LangElement>*/ typename E>
		friend class AccessLocked;
		friend class LangElement;
		template<std::derived_from<LangElement> T>
		friend struct GenericPSTQueryKey;

		AccessLocked(MCRef<Element> ref): ref(ref) {}

	public:
		AccessLocked()                    = delete;
		AccessLocked(const AccessLocked&) = default;
		AccessLocked(AccessLocked&&)      = default;

		template<typename E>
		AccessLocked(const Access<E> oth) noexcept: ref(oth.ref) {}

		template<typename T>
		AccessLocked(const AccessLocked<T> other) noexcept: ref(other.ref) {}

		/**
		 * @brief Access that doesn't require passing context. It should only be used outside of
		 * queries unless for debuging purposes
		 */
		auto illegalAccess() const {
			return ref.toOpt().map([](CRef<Element> ref) -> Access<Element> { return { ref }; });
		}

		/**
		 * @brief This should be fine for now, casting might end up as null which would be
		 * potentially bad for knowing about accesses.
		 */
		template<typename T>
		AccessLocked<T> dynamicCast() const {
			return { { dynamic_cast<const T*>(&*ref) } };
		}

		/**
		 * @brief Unlock an access safely returning an optional. Notifies the query framework about
		 * the access.
		 */
		base::Optional<Access<Element>> unlockOpt(query::Context& ctx) const {
			// hmm:...
			if (ref.toOpt().has_value()) {
				detail::notifyContext(ctx, this->ref->getID() );
			}
			return ref.toOpt().map([](CRef<Element> ref) -> Access<Element> { return { ref }; });
		}

		/**
		 * @brief Unlock an access unsafely. Notifies the query framework about the access and
		 * throws if the element was null.
		 */
		Access<Element> unlock(query::Context& ctx) const {
			if (!ref.toOpt()) detail::notifyBadAccess(ctx);
			detail::notifyContext(ctx, this->ref->getID());
			return { ref.toOpt().value() };
		}

		template<typename E>
		AccessLocked& operator=(const Access<E>& oth) noexcept {
			ref = oth.ref;
			return *this;
		}

		template<typename E>
		AccessLocked& operator=(AccessLocked<E>&& oth) noexcept {
			ref = std::move(oth).ref;
			return *this;
		}
	};

	/**
	 * @brief Wrapper for an optional pst box. It should only be used inside of pst as a way to
	 * store elements.
	 */
	template</*std::derived_from<LangElement>*/ typename Element>
	class AccessInternal final {
	private:
		MBox<Element> box;

		template<typename E>
		friend class AccessInternal;

	public:
		AccessInternal()                               = default;
		AccessInternal(const AccessInternal<Element>&) = default;
		AccessInternal(AccessInternal<Element>&&)      = default;

		AccessInternal(MBox<Element>&& box): box(std::move(box)) {}

		/**
		 * @brief Create a locked access from internal access, meant to be used in getters in pst to
		 * return locked accesses.
		 */
		AccessLocked<Element> give() const { return { box.ref() }; }

		template<typename E>
		AccessInternal& operator=(AccessInternal<E>&& oth) noexcept {
			box = std::move(oth).box;
			return *this;
		}

		template<typename E>
		AccessInternal& operator=(MBox<E>&& mbox) noexcept {
			box = std::move(mbox);
			return *this;
		}

		/**
		 * @brief For internal usage of an Element, the alternative is to make Element a friend of
		 * AccessInternal but it allows for easier access leaks.
		 */
		MCRef<Element> internal() const { return box.ref(); }
	};
}

#define VISITOR_ACCESS_METHOD_INTERFACE(type) void visit##type(pst::Access<type>)

/**
 * @brief Macro to create a visitor that uses accesses as arguments passed to visitors.
 */
#define MAKE_ACCESS_VISITOR(name, ...) \
	MAKE_VISITOR_CUSTOM_INTERFACE(name, VISITOR_ACCESS_METHOD_INTERFACE, __VA_ARGS__)
