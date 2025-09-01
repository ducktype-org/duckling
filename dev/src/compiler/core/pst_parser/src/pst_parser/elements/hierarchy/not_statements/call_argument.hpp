#pragma once

#include "../meta.hpp"

#include <base/optional.hpp>

namespace pst {
	/**
	 * @brief Attribute element, can be before any statement. TODO
	 */
	class CallArgument final: public NotStmt {
	public:                                                      // TODO
		base::Optional<tpc::Identifier>               arg_name;  // exists only in named args.
		AccessInternal<UniversalExprHolderLowerLevel> arg;

	public:
		explicit CallArgument(const dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::CallArgument;
		}

		static MBox<CallArgument> parse(LangParserState& state);
		~CallArgument() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Call argument";
		}
	};
}
