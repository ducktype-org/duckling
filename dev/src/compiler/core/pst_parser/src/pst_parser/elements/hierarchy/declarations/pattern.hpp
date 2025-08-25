#pragma once

#include "../lists/parameter_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Pattern declaration
	 */
	class Pattern final: public Decl {
		tpc::Identifier                                 name;
		AccessInternal<Param>                           param;
		base::Optional<AccessInternal<CommaExprHolder>> ret;
		AccessInternal<CodeBlockOrStmt>                 body;

	public:
		DECL_CHILD_CONSTRUCTOR(Pattern, ElementKind::Pattern);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<Param> getParam() const {
			return param.give();
		}

		/**
		 * @note Optional of MCRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getRet() const {
			return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		static MBox<Pattern> parse(LangParserState& state);
		void                 dprint(std::ostream& out) const final;
		~Pattern() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
