#pragma once


#include <base/extend_cpp/stringifyable_enum.hpp>

namespace pst {
    enum class StmtKind : int;
}

namespace compiler::helios {

	enum class Attribute { DVMOnlyImpl, NativeOnlyImpl, BackendDependent };

    /**
     * @brief Check if the given attribute can be applied to given stmt.
     */
	bool isValidForStmt(Attribute attr, pst::StmtKind kind);

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
