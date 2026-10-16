#include "formatter.hpp"

#include "doc_builder.hpp"
#include "doc_renderer.hpp"
#include "statement_tree.hpp"

#include <base/except/exceptions.hpp>

#include <vector>

namespace formatter {
	namespace {

		/**
		 * Both the structure pass and the builder recurse once per bracket level, so a
		 * pathological nesting depth would exhaust the stack. Real sources stay far below this.
		 */
		constexpr usize MAX_NESTING_DEPTH = 512;

		/** Walks the bracket tree iteratively, so the check itself cannot overflow. */
		void checkNestingDepth(const lexer::Tokens& tokens) {
			struct Frame final {
				const lexer::Tokens* tokens;
				usize                index;
			};

			std::vector<Frame> stack{ { .tokens = &tokens, .index = 0 } };
			while (!stack.empty()) {
				Frame& frame = stack.back();
				if (frame.index >= frame.tokens->size()) {
					stack.pop_back();
					continue;
				}
				const lexer::Token& token = (*frame.tokens)[frame.index++];
				if (token.getType() != lexer::Token::Type::BracketGroup) continue;
				if (stack.size() >= MAX_NESTING_DEPTH)
					throw base::LogicError("source nesting is too deep to format");
				stack.push_back({ .tokens = &token.getRecursive(), .index = 0 });
			}
		}
	}

	std::string formatTokens(const lexer::TokenData& tokens, const FormatConfig& config) {
		checkNestingDepth(tokens.tokens);

		const auto statements = parseStatementList(tokens.tokens);
		if (statements.empty()) return "";

		std::string out = renderDoc(buildFileDoc(statements, config), config);
		out += '\n';
		return out;
	}
}
