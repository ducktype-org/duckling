#pragma once
#include <concepts>

namespace hashing {


template<typename T>
concept hash_algorithm = requires {
    std::is_object_v<T>;
    std::is_constructible_v<T>;
    std::is_destructible_v<T>;

    std::is_invocable_v<T, void*, usize>;
    typename T::result_type;
    std::is_convertible_v<T, typename T::result_type>;
};

template<typename T>
concept has_update_hash = requires(T t, void* data, usize len) {
    t.update_hash(data, len);
};

class call_overloads {
public:
    /*constexpr*/ decltype(auto) operator()(this has_update_hash auto&& self, const volatile void* data, usize len) noexcept {
        self.update_hash(const_cast<void*>(data), len);
        return std::forward<decltype(self)>(self);
    }

    template<typename T>
    requires (std::has_unique_object_representations_v<T> && !std::ranges::input_range<T>)
    /*constexpr*/ decltype(auto) operator()(this has_update_hash auto&& self, const T& t) noexcept {
        self.update_hash(std::addressof(t), sizeof(t));
        return std::forward<decltype(self)>(self);
    }

    template<std::ranges::input_range R>
    requires std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
    /*constexpr*/ decltype(auto) operator()(this has_update_hash auto&& self, R&& range) noexcept(noexcept(std::ranges::data(range), std::ranges::size(range))) {
        if constexpr(std::ranges::contiguous_range<R>) {
            self.update_hash(std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>));
        }
        else {
            for(auto&& elem : range) {
                self.update_hash(std::addressof(elem), sizeof(elem));
            }
        }
        return std::forward<decltype(self)>(self);
    }
};


} // namespace hashing
