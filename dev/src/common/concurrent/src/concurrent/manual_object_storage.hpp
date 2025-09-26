// @TODO: move to base

#pragma once

#include <new>
#include <utility>

namespace concurrent {

	/**
	 * Lifetime management for a single object.
	 * Note: destructor must be called manually.
	 * Any wrong usage results in undefined behavior.
	 * See: https://en.cppreference.com/w/cpp/utility/launder.html
	 */
	template<class T>
	struct ObjStorage {
		// @TODO: // optional can be used in dev builds with assertion in destructor

	private:
		alignas(T) std::byte data[sizeof(T)];

	public:
		template<class... Args>
		void construct(Args&&... args) {
			new (data) T(std::forward<Args>(args)...);
		}

		void destroy() { this->get()->~T(); }

		T* get() { return std::launder(reinterpret_cast<T*>(&data)); }
	};

}
