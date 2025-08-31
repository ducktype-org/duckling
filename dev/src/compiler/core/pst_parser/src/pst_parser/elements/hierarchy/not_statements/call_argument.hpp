#pragma once

#include <base/optional.hpp>

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Attribute element, can be before any statement. TODO
	 */
	class CallArgument final: public NotStmt {
	public: // TODO
		base::Optional<tpc::Identifier> arg_name; // exists only in named args.
		AccessInternal<UniversalExprHolderLowerLevel> arg;

	public:
		explicit CallArgument(const dia::SourcePosition& pos): NotStmt(pos) {}

		static MBox<CallArgument> parse(LangParserState& state);
		~CallArgument() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Call argument";
		}

        void acceptVisitor(PstVisitor& visitor) const final;
	};
}
