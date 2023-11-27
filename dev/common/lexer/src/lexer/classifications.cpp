#include "classifications.hpp"

#include <base/exceptions.hpp>
#include <base/init_guard.hpp>

#include <unicode/utypes.h>
#include <unicode/errorcode.h>
#include <unicode/ustream.h>
#include <cassert>
#include <iostream>

namespace lexer {
	void createFromPattern (icu::UnicodeSet& set, const std::u8string& pattern) {
		icu::ErrorCode err;
		set.applyPattern(icu::UnicodeString::fromUTF8(pattern), err);
		if (err.isFailure()) {
			std::string strerror;
			icu::UnicodeString::fromUTF8(pattern).toUTF8String(strerror);
			RIFT_PANIC("error in unicode set pattern: " + strerror);
		}
		set.freeze();
		return;
	}

	bool checkEmptyIntersect(icu::UnicodeSet& a, icu::UnicodeSet& b) {
		icu::UnicodeSet c;
		c.addAll(a).retainAll(b);
		return (c.size() == 0);
	}

	/**
	 * @brief helper function for printing UnicodeSet information
	 */
	void printUnicodeSetRanges(icu::UnicodeSet& uset) {
		std::cerr << "size: " << uset.size() <<  "\n";
        icu::UnicodeString ustring;
        uset.toPattern(ustring, true);
        std::cerr << "pattern: " << ustring << "\n";
		UChar32 rb, re;
        for(int32_t rangeid = 0; rangeid < uset.getRangeCount(); rangeid++) {
            rb = uset.getRangeStart(rangeid);
            re = uset.getRangeEnd(rangeid);
			std::cerr << "range: " << std::hex << rb << ".." << re << std::dec << "\n";
        }
	}

	void sanityChecks() {
		using Class = Classifications;
		for (auto cl: Class::classes) {
			assert(!cl->isEmpty());
		}
		for (auto cl: Class::classes) {
			assert(cl->isFrozen());
		}
		assert(Class::name_continue.containsAll(Class::name_start));

		assert(checkEmptyIntersect(Class::newline, Class::vertical_space));
		assert(checkEmptyIntersect(Class::newline, Class::format_control));
		assert(checkEmptyIntersect(Class::format_control, Class::vertical_space));

		assert(checkEmptyIntersect(Class::syntax, Class::name_start));
		assert(Class::syntax.containsAll(Class::special));
		assert(Class::syntax.containsAll(Class::operator_start));
		assert(Class::syntax.containsAll(Class::open_bracket));
		assert(Class::syntax.containsAll(Class::close_bracket));

		assert(Class::operator_continue.containsAll(Class::operator_start));

		assert(checkEmptyIntersect(Class::special, Class::operator_continue));

		// name_start and name_continue intersection isn't currently empty. More in Classifications documentation
		// assert(checkEmptyIntersect(Class::name_start, Class::operator_continue));
		icu::UnicodeSet c;
		c.addAll(Class::name_start).retainAll(Class::operator_continue);
		assert(c.size() == 2);

		assert(checkEmptyIntersect(Class::whitespace, Class::name_continue));
		assert(checkEmptyIntersect(Class::whitespace, Class::syntax));
	}

	void Classifications::init () {
		RIFT_SIMPLE_INIT_GUARD_BEGIN
		for (auto cl: classes) {
			assert(!cl->isFrozen());
		}
		createFromPattern(name_start, u8R"([[:XID_Start:][:ID_Compat_Math_Start:][_]])");
		createFromPattern(name_continue, u8R"([[:XID_Continue:][:ID_Compat_Math_Continue:]])");
		createFromPattern(whitespace, u8R"([:Pattern_White_Space:])");
		createFromPattern(newline, u8R"([\u000A-\u000D\u0085\u2028\u2029])");
		createFromPattern(format_control, u8R"([[:Pattern_White_Space:]&[:Default_Ignorable_Code_Point:]])");
		vertical_space.clear().addAll(whitespace).removeAll(newline).removeAll(format_control).freeze();

		createFromPattern(syntax, u8R"([[:Pattern_Syntax:]-[:ID_Compat_Math_Continue:]])");
		createFromPattern(special, u8R"([;$@#,'"])");
		createFromPattern(open_bracket, u8R"([[:Pattern_Syntax:]&[:Bidi_Paired_Bracket_Type=Open:]])");
		createFromPattern(close_bracket, u8R"([[:Pattern_Syntax:]&[:Bidi_Paired_Bracket_Type=Close:]])");

		operator_start.clear()
						.addAll(syntax)
						.removeAll(special)
						.removeAll(open_bracket)
						.removeAll(close_bracket)
						.freeze();
						
		icu::ErrorCode err;
		operator_continue.applyPattern(icu::UnicodeString::fromUTF8(u8R"([:Mn:])"), err).addAll(operator_start).freeze();
		if (err.isFailure()) { RIFT_PANIC(std::string(err.errorName())); }

		end_of_file.add(end_of_file_value).freeze();

		sanityChecks();
		RIFT_SIMPLE_INIT_GUARD_END
	}
}