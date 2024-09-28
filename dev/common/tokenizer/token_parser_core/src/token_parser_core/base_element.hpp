#pragma once

#include "parser_ref.hpp"
#include <base/ints.hpp>
#include <ostream>

namespace tpc {
	/**
	 * @brief Base class for implementations of AST nodes
	 *
	 * Things that can parse themself should have this method:
	 *  - static ParserRef<Element> parse(RiftParserState& state);
	 */

	class Element {
	public:
		/**
		 * @brief Method used to print information from AST in a format similar to JSON
		 */
		virtual void dprint(std::ostream& out) const = 0;
		// virtual void semPrint(std::ostream& out) const = 0;
		virtual ~Element() = 0;

		// @IDEA: this might be just a const variable if it will be enough in the future
		/**
		 * @brief function providing information whether this kind of element should end in a
		 * semicolon
		 *
		 * @todo consider moving semicolon requirement to statement (Stmt) parsing.
		 */
		[[noreturn]]
		virtual bool trailingSemicolon();

		// @IDEA perhaps add virtual final, so no one can override it
		inline void* operator new(usize size) {
			// placeholder for future custom allocation
			return ::operator new(size);
		}

		inline void operator delete(void* p) {
			// placeholder for future custom allocation
			return ::operator delete(p);
		}
	};

	template<typename T, typename State>
	concept ParseAbleElement = requires(State& state) {
		requires std::derived_from<T, Element>;
		{ T::parse(state) } -> std::same_as<ParserRef<T>>;
	};
}
