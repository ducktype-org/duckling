#pragma once

#include <new>
#include <utility>
#include <base/comptime/type_traits.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>

namespace base {

	/**
	 * Explicit lifetime management for a single object.
     *
     * It is not aware of any of the object semantics, it just stores see raw bytes and allows to
     * construct and destroy the object in place.
     *
	 * Any wrong usage results in undefined behavior.
	 * See also: https://en.cppreference.com/w/cpp/utility/launder.html
	 */
	template<class T>
    requires base::IsPlainType<T>
	struct ManualLifetimeStorage final {
	private:
		alignas(T) std::byte data[sizeof(T)] = {}; // NOLINT

        enum class State { Empty, Constructed };
        IF_BUILD_TYPE_DEV(State state = State::Empty;)

	public:
        ManualLifetimeStorage() = default;

        ManualLifetimeStorage(const ManualLifetimeStorage&) = delete;
        ManualLifetimeStorage(ManualLifetimeStorage&&) = delete;
        ManualLifetimeStorage& operator=(const ManualLifetimeStorage&) = delete;
        ManualLifetimeStorage& operator=(ManualLifetimeStorage&&) = delete;

        // We set it explicitly, to make sure that in release builds destructor is trivial:
        IF_BUILD_TYPE_RELEASE(
            ~ManualLifetimeStorage() noexcept = default;
        )
        IF_BUILD_TYPE_DEV(
            ~ManualLifetimeStorage() RELEASE_NOEXCEPT {
                CORE_ASSERT(state == State::Empty, "Object is still constructed during destruction of ManualLifetimeStorage");
            }
        )


		template<class... Args>
		void construct(Args&&... args) {
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Empty, "Object is already constructed");
                state = State::Constructed;
            })
			new (data) T(std::forward<Args>(args)...);
		}

		void destroy() {
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Constructed, "Object is not constructed");
                state = State::Empty;
            })
            this->get()->~T();
        }

		T* get() { 
            IF_BUILD_TYPE_DEV({
                CORE_ASSERT(state == State::Constructed, "Object is not constructed");
            })
            return std::launder(reinterpret_cast<T*>(&data));
        }
	};

}