#include "formatter.hpp"

#include "doc_builder.hpp"
#include "doc_renderer.hpp"
#include "statement_tree.hpp"

namespace formatter {

	std::string formatTokens(const lexer::TokenData& tokens, const FormatConfig& config) {
		const auto statements = parseStatementList(tokens.tokens);
		if (statements.empty()) return "";

		std::string out = renderDoc(buildFileDoc(statements, config), config);
		out += '\n';
		return out;
	}
}
