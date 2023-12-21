#include "../convert.hpp"

namespace base {
	std::string toHexString(usize hex, usize length) {
		if (length == 0) {
			usize cp = hex;
			while(cp) {
				cp /= 16;
				length++;
			}
			if (length == 0) {
				length = 1;
			}
		}
		std::string out(length + 2, '0');
		out[1] = 'x';
		usize i = 0;
		usize rest;
		while (i < length && hex) {
			rest = hex % 16;	
			out.at(length + 1 - i) = (rest > 9) ? ('A' + rest - 10) : ('0' + rest);
			hex = hex / 16;
			i++;
		} 
		return out;
	}
}