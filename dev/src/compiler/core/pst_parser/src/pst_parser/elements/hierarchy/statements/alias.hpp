#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Alias statement.
	 */
	class Alias final: public Stmt {
		tpc::Identifier name;
		NAMED_CHILD(points_to, DottedName);

	public:
		STMT_CHILD_CONSTRUCTOR(Alias, ElementKind::Alias);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<DottedName> getPointed() const {
			return points_to.give();
		}

		static MBox<Alias> parse(LangParserState& state);
		~Alias() final = default;
		void dprint(std::ostream& out) const final;
		u64 calcStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Alias";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return name.value;
		}
	};
}
