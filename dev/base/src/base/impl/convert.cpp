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
		usize i = 0, r;
		while (i < length && hex) {
			r = hex % 16;	
			out[length + 1 - i] = (r > 9) ? ('A' + r - 10) : ('0' + r);
			hex = hex / 16;
			i++;
		} 
		return out;
	}
}