// @TODO: move to base

#pragma once

#include <utility>
#include <new>

namespace concurrent {

    /**
     * Lifetime management for a single object.
     * Note: destructor must be called manually.
     * See: https://en.cppreference.com/w/cpp/utility/launder.html
     * @TODO: does it work for polymorphic types? Does it work at all?
     */
    template<class T>
    struct ObjStorage {
    private:
        alignas (T) std::byte data[sizeof(T)];

    public:
        template<class... Args>
        void construct(Args&&... args) {
            new (data) T(std::forward<Args>(args)...);
        }

        void destroy() {
            this->get()->~T();
        }

        T* get() {
            return std::launder(reinterpret_cast<T*>(&data));
        }
    };


    
}