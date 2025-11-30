#pragma once

#include <base/types/ints.hpp>
#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <set>
#include <string>

namespace dia_int::term_ui_view {
	enum class StyleType { Error, Warning, Note, Hint, Docs };

	struct PointerMessage {
		std::string text;
		StyleType   type;
		u64         priority;
	};

	struct CodePiece {
		std::string   text;
		std::set<u64> pointer_ids;
	};

	struct CodeLine {
		base::Optional<u64>    line_no;
		std::vector<CodePiece> pieces;
	};

	struct CodeSection {
		std::string                        file;
		u64                                line;
		u64                                col;
		std::vector<CodeLine>              lines;
		base::HashMap<u64, PointerMessage> pointers;
	};

	using TextSection = std::string;
	using Section     = std::variant<TextSection, CodeSection>;

	struct Message {
		StyleType            type;
		u64                  code;
		std::string          header;
		std::vector<Section> sections;
	};

	struct Diagnostic {
		std::vector<Message> messages;
	};
}
