#pragma once

#include <frontend/pst_parser/elements/hierarchy/stmt_kind_fd.hpp>

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>

namespace compiler::helios {
	namespace attributes {
		struct BackendDependent {};

		struct DVMOnlyImpl {};

		struct NativeOnlyImpl {};

	}

	using Attribute
		= std::variant<attributes::DVMOnlyImpl, attributes::NativeOnlyImpl, attributes::BackendDependent>;

	/**
	 * @brief Check if the given attribute can be applied to given stmt.
	 */
	bool isValidForStmt(Attribute attr, pst::StmtKind kind);

	/**
	 * @brief Return true if a given attribute disables lookup on symbol
	 * the attribute is applied for.
	 */
	bool disablesLookup(Attribute attr);

	/**
	 * @brief Validates if the combination of attributes is correct.
	 * Returns std::unexpected state if they are incorrect with error message.
	 */
	std::expected<std::monostate, std::string> validateAttributes(
		const std::vector<Attribute>& attributes
	);

	base::Optional<Attribute> attrFromStr(base::StrID str);

	base::StrID attrToStr(Attribute attr);
}
