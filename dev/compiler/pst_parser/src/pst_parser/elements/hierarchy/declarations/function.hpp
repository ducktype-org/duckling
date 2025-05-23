#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration
	 */
	class Fun final: public Decl {
		tpc::Identifier                                 name;
		AccessInternal<ParamList>                       params;
		base::Optional<AccessInternal<CommaExprHolder>> ret;
		AccessInternal<CodeBlockOrStmt>                 body;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun, ElementKind::Fun);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		/**
		 * @note Optional of MCRef here is intentional
		 */
		base::Optional<AccessLocked<ExprHolder>> getRet() const {
			return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		static MBox<Fun> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~Fun() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
