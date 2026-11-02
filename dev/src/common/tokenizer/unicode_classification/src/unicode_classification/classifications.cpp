// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "classifications.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/init_guard.hpp>

#include <init/init.hpp>

#include <unicode/errorcode.h>
#include <unicode/uclean.h>
#include <unicode/ustream.h>
#include <unicode/utypes.h>

#include <iostream>

namespace unicode {

	icu::UnicodeSet Classifications::name_start;
	icu::UnicodeSet Classifications::name_continue;
	icu::UnicodeSet Classifications::operator_start;
	icu::UnicodeSet Classifications::operator_continue;
	icu::UnicodeSet Classifications::vertical_space;
	icu::UnicodeSet Classifications::newline;
	icu::UnicodeSet Classifications::whitespace;
	icu::UnicodeSet Classifications::format_control;
	icu::UnicodeSet Classifications::special;
	icu::UnicodeSet Classifications::syntax;
	icu::UnicodeSet Classifications::open_bracket;
	icu::UnicodeSet Classifications::close_bracket;
	icu::UnicodeSet Classifications::end_of_file;

	/**
	 * @brief Create unicode set from an u8 pattern, handles the errorcode and freezes the set
	 *
	 * The pattern is in the icu::UnicodeSet::applyPattern format.
	 */
	void createFromPattern(icu::UnicodeSet& set, const std::u8string& pattern) {
		icu::ErrorCode err;
		set.applyPattern(icu::UnicodeString::fromUTF8(pattern), err);
		if (err.isFailure()) {
			std::string strerror;
			icu::UnicodeString::fromUTF8(pattern).toUTF8String(strerror);
			CORE_PANIC("error in unicode set pattern: " + strerror);
		}
		set.freeze();
		return;
	}

	/**
	 * @brief Checks whether the intersection of two sets is empty
	 */
	bool checkEmptyIntersect(icu::UnicodeSet& a, icu::UnicodeSet& b) {
		icu::UnicodeSet c;
		c.addAll(a).retainAll(b);
		return (c.size() == 0);
	}

	/**
	 * @brief helper function for printing UnicodeSet information
	 */
	void printUnicodeSetRanges(icu::UnicodeSet& uset) {
		std::cerr << "size: " << uset.size() << "\n";
		icu::UnicodeString ustring;
		uset.toPattern(ustring, true);
		// Convert to UTF-8 rather than using icu's operator<<(ostream&, ...): that operator
		// lives in libicuio built against libc++, which is ABI-incompatible with this
		// libstdc++ build; toUTF8String is a header template compiled with our std lib.
		std::string ustring_utf8;
		ustring.toUTF8String(ustring_utf8);
		std::cerr << "pattern: " << ustring_utf8 << "\n";
		UChar32 rb = 0, re = 0;
		for (int32_t rangeid = 0; rangeid < uset.getRangeCount(); rangeid++) {
			rb = uset.getRangeStart(rangeid);
			re = uset.getRangeEnd(rangeid);
			std::cerr << "range: " << std::hex << rb << ".." << re << std::dec << "\n";
		}
	}

	/**
	 * @brief Makes some checks on generated sets that might be useful when making changes or
	 * changing the icu version
	 */
	void sanityChecks() {
		using Class = Classifications;
		for (auto cl: Class::CLASSES) CORE_ASSERT(!cl->isEmpty(), "a classification is empty");
		for (auto cl: Class::CLASSES) CORE_ASSERT(cl->isFrozen(), "a classification isn't frozen");

		CORE_ASSERT(
			Class::name_continue.containsAll(Class::name_start),
			"Set of characters continuing a name should include all the characters starting a name."
		);

		CORE_ASSERT(
			checkEmptyIntersect(Class::newline, Class::vertical_space),
			"Sets of newline characters and vertical space characters should be disjoint."
		);
		CORE_ASSERT(
			checkEmptyIntersect(Class::newline, Class::format_control),
			"Sets of newline characters and format control characters should be disjoint."
		);
		CORE_ASSERT(
			checkEmptyIntersect(Class::format_control, Class::vertical_space),
			"Sets of format control characters and vertical space characters should be disjoint."
		);

		CORE_ASSERT(
			checkEmptyIntersect(Class::syntax, Class::name_start),
			"Sets of syntax characters and characters starting a name should be disjoint"
		);
		CORE_ASSERT(
			Class::syntax.containsAll(Class::special),
			"Special characters should be a subset of syntax characters"
		);
		CORE_ASSERT(
			Class::syntax.containsAll(Class::operator_start),
			"Characters starting an operator should be a subset of syntax characters"
		);
		CORE_ASSERT(
			Class::syntax.containsAll(Class::open_bracket),
			"Open brackets should be a subset of syntax characters"
		);
		CORE_ASSERT(
			Class::syntax.containsAll(Class::close_bracket),
			"Closed brackets should be a subset of syntax characters"
		);

		CORE_ASSERT(
			Class::operator_continue.containsAll(Class::operator_start),
			"Set of characters continuing an operator should include all the characters starting "
			"an operator."
		);

		CORE_ASSERT(
			checkEmptyIntersect(Class::special, Class::operator_continue),
			"Sets of special characters and characters continuing an operator should be disjoint."
		);

		// name_start and name_continue intersection isn't currently empty. More in Classifications
		// documentationassertTrue(checkEmptyIntersect(Class::name_start,
		// Class::operator_continue));
		icu::UnicodeSet c;
		c.addAll(Class::name_start).retainAll(Class::operator_continue);
		CORE_ASSERT(
			c.size() == 2,
			"Intersection of Characters starting a name and characters continuing an operator "
			"should be known."
		);

		CORE_ASSERT(
			checkEmptyIntersect(Class::whitespace, Class::name_continue),
			"Whitespace characters and characters continuing a name should be disjoint."
		);
		CORE_ASSERT(
			checkEmptyIntersect(Class::whitespace, Class::syntax),
			"Whitespace characters and syntax characters should be disjoint."
		);
	}

	void Classifications::init() {
		SIMPLE_INIT_GUARD_BEGIN
		for (auto cl: CLASSES)
			CORE_ASSERT(!cl->isFrozen(), "a classification is frozen at the beginning");
		createFromPattern(name_start, u8R"([[:XID_Start:][:ID_Compat_Math_Start:][_]])");
		createFromPattern(name_continue, u8R"([[:XID_Continue:][:ID_Compat_Math_Continue:]])");
		createFromPattern(whitespace, u8R"([:Pattern_White_Space:])");
		createFromPattern(newline, u8R"([\u000A-\u000D\u0085\u2028\u2029])");
		createFromPattern(
			format_control, u8R"([[:Pattern_White_Space:]&[:Default_Ignorable_Code_Point:]])"
		);
		vertical_space.clear()
			.addAll(whitespace)
			.removeAll(newline)
			.removeAll(format_control)
			.freeze();

		createFromPattern(syntax, u8R"([[:Pattern_Syntax:]-[:ID_Compat_Math_Continue:]])");
		createFromPattern(special, u8R"([;$@#,'"])");
		createFromPattern(
			open_bracket, u8R"([[:Pattern_Syntax:]&[:Bidi_Paired_Bracket_Type=Open:]])"
		);
		createFromPattern(
			close_bracket, u8R"([[:Pattern_Syntax:]&[:Bidi_Paired_Bracket_Type=Close:]])"
		);

		operator_start.clear()
			.addAll(syntax)
			.removeAll(special)
			.removeAll(open_bracket)
			.removeAll(close_bracket)
			.freeze();

		icu::ErrorCode err;
		operator_continue.applyPattern(icu::UnicodeString::fromUTF8(u8R"([:Mn:])"), err)
			.addAll(operator_start)
			.freeze();
		if (err.isFailure()) CORE_PANIC(std::string(err.errorName()));

		end_of_file.add(END_OF_FILE_VALUE).freeze();

		sanityChecks();
		SIMPLE_INIT_GUARD_END
	}

	// This call we keep in cpp files,
	// since it strictly realted to Classifications class
	// which is implemented here.
	RUN_BEFORE_MAIN(init::registerForInit(Classifications::init));

	/**
	 * A wrapper around u_cleanup macro
	 */
	void cleanupICU() { u_cleanup(); }

	RUN_BEFORE_MAIN(init::registerForDeinit(cleanupICU));
}
