#pragma once

#include <base/ints.hpp>
#include <ostream>

namespace tpc {
	class Element {
	public:
		/** Things that can parse themself have this method: */
		// static ParserRef<Element> parse(RiftParserState& state);

		virtual void dprint(std::ostream &out) const = 0;
		virtual ~Element()                           = 0;

		// @IDEA: this might be just a const variable if it will be enough in the future
		[[noreturn]]
		virtual bool trailingSemicolon();

		// @IDEA perhaps add virtual final, so no one can override it
		inline void *operator new(usize size) {
			// placeholder for future custom allocation
			return ::operator new(size);
		}

		inline void operator delete(void *p) {
			// placeholder for future custom allocation
			return ::operator delete(p);
		}
	};
}
