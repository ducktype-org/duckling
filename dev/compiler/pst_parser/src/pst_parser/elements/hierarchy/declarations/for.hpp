#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief For declaration
	 */
	class For final: public CodeDecl {
		tpc::OptionalIdentifier           optional_name;
		tpc::Identifier                   iterator;
		AccessInternal<ForTypeExprHolder> type;
		AccessInternal<CommaExprHolder>   iterable;
		AccessInternal<CodeBlockOrStmt>   body;

	public:
		explicit For(const dia::SourcePosition& position): CodeDecl(position) {}

		static MBox<For> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~For() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "For";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
