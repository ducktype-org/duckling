#include "regex.hpp"

#include <regex>


namespace regex {
    struct RegexImpl {
        std::regex regex;

        RegexImpl(std::regex regex): regex(std::move(regex)) {}
    };
}

namespace base::extend {
	void BoxPtrDeleter<regex::RegexImpl>::del(
		regex::RegexImpl* ptr
	) {
		delete ptr;
	}
}

namespace regex {
    Regex::Regex(std::string_view pattern): impl(makeBox<RegexImpl>(std::regex{pattern.cbegin(), pattern.cend()})) {}
    Regex::~Regex() = default;

    Regex::Regex(const Regex& oth):
        impl{makeBox<RegexImpl>(oth.impl->regex)} {}

    bool Regex::match(std::string_view str) const {
        return std::regex_match(str.begin(), str.end(), impl->regex);
    }
}

