#pragma once

#include "../lists/parameter_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration
	 */
	class FunDecl final: public Decl {
		tpc::Identifier name;
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD_OPT(ret, CommaExprHolder);

	public:
		DECL_CHILD_CONSTRUCTOR(FunDecl, ElementKind::FunDecl);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		bool trailingSemicolon() override {
			return true;
		}

		[[nodiscard]]
		/**
		 * @note Optional of MCRef here is intentional
		 */
		base::Optional<AccessLocked<ExprHolder>> getRet() const {
			return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
		}

		static MBox<FunDecl> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~FunDecl() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return getName();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
