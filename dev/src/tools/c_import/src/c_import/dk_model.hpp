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
	};

	/// An `extern("C") class`.
	struct DkClass final {
		std::string             name;
		std::vector<DkNameType> fields;
		std::string             comment;
	};

	/// An `extern("C") fundecl`; a missing return type is C `void`.
	struct DkFundecl final {
		std::string                name;
		std::vector<DkNameType>    params;
		std::optional<std::string> return_type;
	};

	struct DkConst final {
		std::string name;
		std::string type;
		std::string value;
	};

	/// A generated Duckling function, such as a view or an accessor of a layout blob.
	struct DkFun final {
		std::string              name;
		std::vector<DkNameType>  params;
		std::string              return_type;
		std::vector<std::string> body;
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
