#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Declaration of a single function argument.
	 */
	class FunParam final: public NotStmt {
		tpc::Identifier                                     name;
		AccessInternal<UniversalExprHolder>                 type;
		base::Optional<AccessInternal<UniversalExprHolder>> initial;

	public:
		explicit FunParam(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::FunParam;
		}

		static MBox<FunParam> parse(LangParserState& state);
		~FunParam() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function Parameter";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<UniversalExprHolder>> getValue() const;

		void acceptVisitor(PstVisitor& visitor) const final;
	};
}
