// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "convert.hpp"

namespace base {
	std::string toHexString(usize hex, usize length) {
		if (length == 0) {
			usize cp = hex;
			while (cp) {
				cp /= 16;
				length++;
			}
			if (length == 0) length = 1;
		}
		std::string out(length + 2, '0');
		out[1]     = 'x';
		usize i    = 0;
		usize rest = 0;
		while (i < length && hex) {
			rest = hex % 16;
			if (rest > 9)
				out.at(length + 1 - i) = char('A' + rest - 10);
			else
				out.at(length + 1 - i) = char('0' + rest);
			hex = hex / 16;
			i++;
		}
		return out;
	}
}
