/**
 * Functions from this file are simillar to functions from cpp standard library, but:
 * * they are constexpr,
 * * they don't depend on locale.
 */

#pragma once

namespace base {
	/**
	 * Checks if a character is an ascii digit.
	 */
	constexpr bool isDigit(char c) { return c >= '0' and c <= '9'; }

	/**
	 * Checks if a character is an ascii letter.
	 */
	constexpr bool isAlpha(char c) { return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z'); }

	/**
	 * Checks if a character is an ascii letter or digit.
	 */
	constexpr bool isAlnum(char c) { return isAlpha(c) or isDigit(c); }
}
