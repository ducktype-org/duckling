#pragma once

#include "ints.hpp"

namespace base::detail {
	template<class ID>
	static i64 NEXT_ID = 0;	
}

#define STRONG_TYPEDEF_ID(NAME) \
	class NAME final {                                                                           \
	private:                                                                                     \
		u64 id;                                                                                  \
		inline constexpr NAME() = default;                                                       \
	public:                                                                                      \
		inline constexpr NAME(const NAME& mX)                 = default;                         \
		inline constexpr NAME(NAME&& mX) noexcept             = default;                         \
		inline constexpr NAME& operator=(const NAME& rhs)     = default;                         \
		inline constexpr NAME& operator=(NAME&& rhs) noexcept = default;                         \
        [[nodiscard]] \
		static NAME next() { NAME out; out.id = ::base::detail::NEXT_ID<NAME>++; return out; }   \
		[[nodiscard]] \
		inline constexpr explicit operator u64() const noexcept { return id; }                   \
		[[nodiscard]] \
		inline constexpr u64 asInt() const noexcept { return id; }                               \
		auto operator<=>(const NAME&) const = default;                                           \
	};

#define ID_STD_HASH(TYPE) \
	template<> \
	struct std::hash<TYPE> { \
		usize operator()(const TYPE& key) const { return key.asInt(); } \
	};
