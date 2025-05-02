#pragma once

#include <base/box.hpp>


namespace regex {
    struct RegexImpl;
}

namespace base::extend {
	/**
	 * @brief Custom Box/MBox deleter for RegexImpl.
	 * It is needed to avoid UB with delete on incomplete type.
	 */
	template<>
	struct BoxPtrDeleter<regex::RegexImpl> {
		static void del(regex::RegexImpl* ptr);
	};
}

namespace regex {
    struct Regex {
        Box<RegexImpl> impl;

        Regex(const Regex&);
        Regex(Regex&&) = default;

        Regex(std::string_view pattern);
        ~Regex();

		/**
		 * @brief Checks if the hole string matches the regex.
		 */
		[[nodiscard]]
		bool match(std::string_view str) const;
    };
}