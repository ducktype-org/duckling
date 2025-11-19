#pragma once
#include <proto/view.pb.h>

#include <base/ints.hpp>

#include <set>
#include <vector>

namespace term_ui {

	/**
	 * @brief A list of text pieces generated from a view manager component.
	 *
	 */
	class TextPieces {
		// The serialized text pieces.
		std::vector<std::string> pieces;

	public:
		// Construct text pieces from a view manager non-highlighted component.
		TextPieces(const view::NoHlComponent& component);

		// Serialize this list by concatenating its elements.
		std::string to_string() const;

	private:
		// Recursively add text pieces to this list from a given component.
		// Used by the constructor.
		void add_component(const view::NoHlComponent& component);
	};

	/**
	 * @brief A piece of code with highlight metadata generated from a view
	 * manager code component.
	 *
	 */
	class CodePiece {
		// The code text.
		std::string text;
		/**
		 * @brief The set of highlight pointer_messages this code piece belongs to
		 *  (aka to which highlights it refers to).
		 */
		std::set<u32> pointer_messages;

	public:
		// Construct a code piece from a view manager highlighted code
		// component.
		CodePiece(const view::HlCodeComponent& component);

		// Get the highlight pointer_messages data.
		const std::set<u32>& getpointer_messages() const;

		// Get the code text.
		const std::string& getText() const;

		// Construct a code piece without a gRPC object. For testing only.
		CodePiece(std::string text);
		// Construct a code piece without a gRPC object. For testing only.
		CodePiece(std::string text, std::set<u32> pointer_messages);
	};

	/**
	 * @brief A list of code pieces generated from a view manager highlighted
	 * component.
	 *
	 */
	class CodePieces {
		// The list of code pieces.
		std::vector<CodePiece> pieces;

	public:
		// Construct a new list from a view manager highlighted component.
		CodePieces(const view::HlComponent& component);

		// Get the list.
		const std::vector<CodePiece>& getPieces() const;

		// Serialize the list by concatenating the elements.
		std::string to_string() const;

	private:
		// Recursively add code pieces to this list from a given component.
		// Used by the constructor.
		void add_component(const view::HlComponent& component);
	};
}
