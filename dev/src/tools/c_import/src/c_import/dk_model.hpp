// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file dk_model.hpp
 * @brief The Duckling declarations a C model lowers to, still as text fragments.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace c_import {

	struct DkNameType final {
		std::string name;
		std::string type;
		/// The C doc comment, emitted above the item; empty without one. The same below.
		std::string doc;
	};

	/// An `extern("C") class`.
	struct DkClass final {
		std::string             name;
		std::vector<DkNameType> fields;
		std::string             comment;
		std::string             doc;
	};

	/// An `extern("C") fundecl`; a missing return type is C `void`.
	struct DkFundecl final {
		std::string                name;
		std::vector<DkNameType>    params;
		std::optional<std::string> return_type;
		/// The linked C name, when it differs from `name`.
		std::optional<std::string> symbol_name;
		std::string                doc;
	};

	struct DkConst final {
		std::string name;
		std::string type;
		std::string value;
		std::string doc;
		/// The doc comment of the C enum the constant starts.
		std::string group_doc;
	};

	/// A generated Duckling function, such as a view or an accessor of a layout blob.
	struct DkFun final {
		std::string              name;
		std::vector<DkNameType>  params;
		std::string              return_type;
		std::vector<std::string> body;
		std::string              doc;
	};

	struct DkSkipped final {
		std::string name;
		std::string reason;
	};

	/// The size and alignment clang reports for a class, checked against Duckling's layout.
	struct DkLayout final {
		std::string   name;
		std::uint64_t size;
		std::uint64_t align;
	};

	struct DkModule final {
		std::vector<DkClass>   classes;
		std::vector<DkFundecl> fundecls;
		std::vector<DkConst>   constants;
		std::vector<DkFun>     functions;
		std::vector<DkSkipped> skipped;
		std::vector<DkLayout>  layouts;
	};

}
