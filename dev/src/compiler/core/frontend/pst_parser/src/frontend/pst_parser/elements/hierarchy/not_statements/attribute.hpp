#pragma once

#include "../lists/attribute_arg_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Attribute element, can be before any statement.
	 */
	class Attribute final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Attribute, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, DottedName);
		NAMED_CHILD(args, AtrArgList);

	public:
		explicit Attribute(LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::Attribute;
		}

		static MBox<Attribute> parse(LangParserState& state);
		~Attribute() final = default;

		[[nodiscard]]
		auto getName() const {
			return name.give();
		}

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
		}
	};
}
