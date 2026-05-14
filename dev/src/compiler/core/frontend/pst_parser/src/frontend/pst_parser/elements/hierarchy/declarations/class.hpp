#pragma once

#include "../expr_holders.hpp"
#include "../lists/implements_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class declaration
	 */
	class Class final: public Decl {
	private:
		tpc::Identifier name;
		NAMED_CHILD(base, ExtendsExprHolder);
		NAMED_CHILD(implements, ImplementsList);
		NAMED_CHILD(body, ClassBlock);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Class, ElementKind::Class);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		tpc::Identifier getNameIdent() const {
			return name;
		}

		[[nodiscard]]
		AccessLocked<ClassBlock> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		AccessLocked<ExtendsExprHolder> getBase() const {
			return base.give();
		}

		[[nodiscard]]
		AccessLocked<ImplementsList> getImplements() const {
			return implements.give();
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return getName();
		}

		static MBox<Class> parse(LangParserState& state);
		~Class() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class";
		}

		// [[nodiscard]]
		// bool isStatementAggregate() const override {
		// 	return true;
		// }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
