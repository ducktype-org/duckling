/**
 * @file go_to_definition.cpp
 * @brief This file defines the findDefinitions function
 */

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios/utils/go_to_definition.hpp>
#include <lsp_interface/go_to_definition.hpp>

#include <base/collections/optional.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <token_source/source.hpp>

#include <format>
#include <string>

namespace lsp {

	std::string cutVfsPrefix(const std::string& uri) {
		const std::string vfs_prefix = "file://vfs:/";
		if (uri.starts_with(vfs_prefix)) {
			// Find the first '/' after "vfs:/<random_string>"
			size_t pos = uri.find('/', vfs_prefix.length());
			if (pos != std::string::npos) {
				// Extract the actual file path and return it as a standard file:// URI
				return "" + uri.substr(pos + 1);
			}
		}
		return uri;  // Return unchanged if it doesn't match the vfs_prefix
	}

	std::string Definition::toJSON() {
		constexpr std::string_view JSON_TEMPLATE = R"-----(
"uri": "{}",
"range": {{
	"start": {{
		"line": {},
		"character": {}
	}},
	"end": {{
		"line": {},
		"character": {}
	}}
}}
)-----";

		return std::format(
			JSON_TEMPLATE,
			cutVfsPrefix(this->uri),
			this->start.first,
			this->start.second,
			this->end.first,
			this->end.second
		);
	}

	Definition::Definition(const pst::LangElement* element) {
		auto source_position = element->getSourcePosition().illegalAccess();
		this->uri   = source_position.getSource()->getFile().getFilePath().toPhysicalPath().uri();
		this->start = source_position.getStartLineColumn();
		this->end   = source_position.getEndLineColumn();
	}

	pst::AccessLocked<pst::LangElement> findElement(
		pst::AccessLocked<pst::LangElement> root, usize offset, bool include_symbold_before_offset
	) {
		auto element = root.illegalAccess().value();

		/**
		 * This is useful for LSP "go to definition" feature, where the cursor can be placed
		 * adjacent to the symbol (like on the right), but in terms of offsets, it is just after the
		 * symbol. Under the assumption that the children are in the right order (from left to right
		 * in the source code), we can include the offsets with 1 more character to the right and
		 * nothing will break because we return when we find the first matching child.
		 */
		usize addend = include_symbold_before_offset ? 1 : 0;

		for (auto sub: element->viewChildren()) {
			auto curr_position = sub.illegalAccess().value()->getSourcePosition().illegalAccess();
			if (curr_position.getStart() <= offset && curr_position.getEnd() + addend >= offset)
				return findElement(sub, offset, include_symbold_before_offset);
		}
		return element;
	}

	base::Optional<Definition> findDefinition(
		pst::AccessLocked<pst::LangElement> element, query::Context& ctx
	) {
		auto pst_expr = element.dynamicCast<pst::ExprElement>();

		base::Optional<Definition> result;
		auto                       sym_id = compiler::helios::querySymIDOfPSTExpr(ctx, pst_expr);
		if (sym_id.has_value()) {
			auto stmt = compiler::helios::stmt(ctx, sym_id.value());
			result    = Definition{ &*stmt.value() };
		}


		return result;
	}
}
