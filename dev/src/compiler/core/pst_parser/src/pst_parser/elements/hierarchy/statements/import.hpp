#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @note: Import allows for two syntaxes right now:
	 * import A.B as D;
	 * import A.B.* as D;
	 *
	 * the optional "star" is ignored.
	 */
	class Import final: public Stmt {
		NAMED_CHILD(names, DottedName);
		tpc::Identifier alias;

	public:
		STMT_CHILD_CONSTRUCTOR(Import, ElementKind::Import);
		static MBox<Import> parse(LangParserState& state);
		[[nodiscard]]
		const decltype(names)& getNames() const;

		[[nodiscard]]
		base::StrID getAlias() const {
			return alias.value;
		}

		/**
		 * @note In the future this functionality will be done by HELIOS.
		 * This functionality is needed to implement early import system for testing.
		 */
		[[nodiscard]]
		std::vector<base::StrID> getModulePath() const;

		[[nodiscard]]
		bool getStar() const;
		~Import() final = default;
		void dprint(std::ostream& out) const final;
		u64 calcStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Import";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return getAlias();
		}
	};
}
