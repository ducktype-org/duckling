#pragma once

#include "../lists/implements_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class declaration
	 */
	class Class final: public Decl {
	private:
		tpc::Identifier                name;
		AccessInternal<ExprElement>    base;
		AccessInternal<ImplementsList> implements;
		AccessInternal<ClassBlock>     body;

	public:
		DECL_CHILD_CONSTRUCTOR(Class, ElementKind::Class);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ClassBlock> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getBase() const {
			return base.give();
		}

		[[nodiscard]]
		AccessLocked<ImplementsList> getImplements() const {
			return implements.give();
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
