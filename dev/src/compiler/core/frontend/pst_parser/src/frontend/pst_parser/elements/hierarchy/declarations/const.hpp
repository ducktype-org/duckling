#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Const compile time variable declaration.
	 */
	class Const final: public Decl {
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD_OPT(type, CommaExprHolder);
		NAMED_CHILD_OPT(value, CommaExprHolder);

		template<typename T, lang_def::Keyword key>
		friend MBox<T> parseVariableTemplate(pst::LangParserState& state);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Const, ElementKind::Const);
		static MBox<Const> parse(LangParserState& state);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getType() const {
			if (type.has_value()) return { type.value().give() };
			return {};
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const {
			if (value.has_value()) return { value.value().give() };
			return {};
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		~Const() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Const";
		}
	};
}
