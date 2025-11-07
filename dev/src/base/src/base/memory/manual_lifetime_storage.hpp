#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/noexcept.hpp>
#include <base/pointers/ref.hpp>

#include <cstddef>
#include <new>
#include <utility>

namespace base {

	/**
	 * Explicit lifetime management for a single object.
	 *
	 * It is not aware of any of the object semantics, it just stores see raw bytes and allows to
	 * construct and destroy the object in place.
	 * For this reason it should be moved in memory.
	 *
	 * Any wrong usage results in undefined behavior.
	 * See also: https://en.cppreference.com/w/cpp/utility/launder.html
	 */
	template<class T>
	requires base::IsPlainType<T>
	struct ManualLifetimeStorage final {  // NOLINT (non-initialization of data in constructor)
	private:
		alignas(T) std::byte data[sizeof(T)] = {};  // NOLINT

		enum class State : bool { Empty, Constructed };
		IF_BUILD_TYPE_DEV(State state = State::Empty;)

		/**
		 * This method exist only to defer the evaluation of the static_assert below,
		 * Since it needs the ManualLifetimeStorage to be a complete type.
		 */
		static void deferred() {
			static_assert(
				std::is_standard_layout_v<ManualLifetimeStorage>,
				"ManualLifetimeStorage should be standard layout"
			);
		}

	public:
		ManualLifetimeStorage() = default;

		ManualLifetimeStorage(const ManualLifetimeStorage&)            = delete;
		ManualLifetimeStorage(ManualLifetimeStorage&&)                 = delete;
		ManualLifetimeStorage& operator=(const ManualLifetimeStorage&) = delete;
		ManualLifetimeStorage& operator=(ManualLifetimeStorage&&)      = delete;

		// We set it explicitly, to make sure that in release builds destructor is trivial:
		IF_BUILD_TYPE_RELEASE(~ManualLifetimeStorage() noexcept = default;)
		IF_BUILD_TYPE_DEV(~ManualLifetimeStorage() {
			CORE_ASSERT_NOEXCEPT(
				state == State::Empty,
				"Object is still constructed during destruction of ManualLifetimeStorage"
			);
		})

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
				CORE_ASSERT(state == State::Constructed, "Object is not constructed (destroy)");
			})
			this->get()->~T();
			// this has to be after destruction, so we can also assert in get():
			IF_BUILD_TYPE_DEV({ state = State::Empty; })
		}

		T* get() {
			IF_BUILD_TYPE_DEV({
				CORE_ASSERT(state == State::Constructed, "Object is not constructed (get)");
			})
			return std::launder(reinterpret_cast<T*>(&data));
		}

		/**
		 * Obtains pointer to the ManualLifetimeStorage from the object reference.
		 * @note Behavior is undefined if obj_ref was not constructed in ManualLifetimeStorage.
		 * Use with caution.
		 *
		 * @note offsetof is conditionally supported for non-standard-layout types since C++17.
		 * If this breaks, figure it out.
		 *
		 * @note I'm not 100% sure the function is well defined here.
		 */
		static Ref<ManualLifetimeStorage> getSelf(Ref<T> obj_ref) {
			constexpr auto OFFSET = offsetof(ManualLifetimeStorage, data);
			static_assert(
				OFFSET == 0,
				"This might not hold actually, but should. Is left here for clarity, and with it "
				"I'm more confident this is UB free."
			);

			auto result = std::launder(reinterpret_cast<ManualLifetimeStorage*>(
				reinterpret_cast<std::byte*>(obj_ref.get()) - OFFSET
			));
			IF_BUILD_TYPE_DEV({
				CORE_ASSERT(
					result->state == State::Constructed, "Object is not constructed (getSelf)"
				);
			})
			return result;
		}
	};

}
