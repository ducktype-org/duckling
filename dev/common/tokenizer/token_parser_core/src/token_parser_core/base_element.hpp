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
	protected:
		/**
		 * @brief Method used to build the printable information in a format similar to JSON
		 *
		 * overrides should generally begin by calling the version from the base class
		 */
		virtual void dprintPrefix(std::ostream&) const {}

		/**
		 * @brief Method used to build the printable information in a format similar to JSON
		 *
		 * overrides should generally end by calling the version from the base class
		 */
		virtual void dprintSuffix(std::ostream&) const {}

		/**
		 * @brief Method used to build the printable information in a format similar to JSON
		 *
		 * It should generally start and end with paired `{}` brackets and contain
		 * only the parts specific to the element.
		 */
		virtual void dprint(std::ostream& out) const = 0;

	public:
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

		/**
		 * @brief Method used to print information from AST in a format similar to JSON
		 *
		 * Constructs it using the dprint and elementType methods.
		 * Can be overriden to provide to allow overriding the whole process.
		 */
		virtual void debugPrint(std::ostream& out) const {
			out << "{";
			dprintPrefix(out);
			out << "\"" << elementType() << "\":";
			dprint(out);
			dprintSuffix(out);
			out << "}";
		}

		/**
		 * @brief Printable element type
		 */
		[[nodiscard]]
		virtual std::string elementType() const {
			return "Element";
		}

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

	template<typename T, typename State, typename... Args>
	concept ParseAbleElement = requires(State& state, Args&&... args) {
		requires std::derived_from<T, Element>;
		{ T::parse(state, std::forward<Args>(args)...) } -> std::same_as<ParserRef<T>>;
	};
}
