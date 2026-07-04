#pragma once

#include <frontend/pst_parser/elements/hierarchy/stmt_kind_fd.hpp>

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>
// Its okay to export this, this is intended
#include <helios/attributes/builtins.hpp>

#include <expected>
#include <vector>

namespace compiler::helios {

#define ATTRIBUTES_LIST                                                                \
	attributes::BackendDependent, attributes::DVMOnlyImpl, attributes::NativeOnlyImpl, \
		attributes::Builtin

	namespace attributes {
		struct BackendDependent {
			bool operator==(const BackendDependent&) const = default;
		};

		struct DVMOnlyImpl {
			bool operator==(const DVMOnlyImpl&) const = default;
		};

		struct NativeOnlyImpl {
			bool operator==(const NativeOnlyImpl&) const = default;
		};

		struct Builtin {
			BuiltinKind builtin;
			bool        operator==(const Builtin&) const = default;
		};
	}

	using Attribute = std::variant<ATTRIBUTES_LIST>;

	template<typename Attr>
	base::Optional<CRef<Attr>> getAttrInVector(const std::vector<Attribute>& attrs);

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

	/**
	 * @brief Convert an attribute name and its arguments into an Attribute.
	 *
	 * Dispatches on the name to a per-attribute parser that validates the argument list, e.g.
	 * `@builtin("ptr_from_slice")`. When the arguments are invalid the parser logs a diagnostic and
	 * fails the current query (never returns).
	 *
	 * @return The parsed attribute, or an empty optional when the name is not a recognized attribute.
	 *
	 * @param args The attribute argument list, empty when the attribute is written without `(...)`.
	 */
	base::Optional<Attribute> attrFromStr(
		query::Context&                                    ctx,
		base::StrID                                        name,
		base::Optional<pst::AccessLocked<pst::AtrArgList>> args
	);

	/**
	 * @brief Get the attribute name.
	 */
	base::StrID attrNameStr(Attribute attr);
}
