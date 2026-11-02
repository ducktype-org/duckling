// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file term_ui_view.hpp
 * @author Wojciech Rzeplinski
 * The term UI view representation of diagnostics.
 * This layer is used to render diagnostics in terminal UI.
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/core/common_classes.hpp>

#include <set>
#include <string>

namespace dia::term_ui_view {
	enum class StyleType { Error, Warning, Note, Hint, Docs };

	struct PointerMessage final {
		std::string text;
		StyleType   type;
		u64         priority;
	};

	struct CodePiece final {
		std::string   text;
		std::set<u64> pointer_ids;
	};

	struct CodeLine final {
		base::Optional<u64>    line_no;
		std::vector<CodePiece> pieces;
	};

	struct CodeSection final {
		CodeLocation                       location;
		std::vector<CodeLine>              lines;
		base::HashMap<u64, PointerMessage> pointers;
	};

	using TextSection = std::string;
	using Section     = std::variant<TextSection, CodeSection>;

	struct Message final {
		StyleType            type;
		u64                  code;
		std::string          header;
		std::vector<Section> sections;
	};

	struct Diagnostic final {
		std::vector<Message> messages;
	};
}
