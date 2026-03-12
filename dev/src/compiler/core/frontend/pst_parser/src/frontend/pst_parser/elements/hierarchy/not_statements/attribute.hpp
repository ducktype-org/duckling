#pragma once

#include "../lists/attribute_arg_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Attribute element, can be before any statement.
	 */
	class Attribute final: public NotStmt {
		NAMED_CHILD(name, DottedName);
		NAMED_CHILD(args, AtrArgList);

	public:
		explicit Attribute(LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::Attribute;
		}

		static MBox<Attribute> parse(LangParserState& state);
		~Attribute() final = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
		}
	};
}
