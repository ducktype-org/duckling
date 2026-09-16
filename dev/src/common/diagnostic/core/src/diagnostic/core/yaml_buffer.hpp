#pragma once

#include <streambuf>

class string_view_streambuf: public std::streambuf {
public:
	string_view_streambuf(std::string_view sv) {
		char* begin = const_cast<char*>(sv.data());  // NOLINT
		char* end   = begin + sv.size();
		this->setg(begin, begin, end);
	}
};
