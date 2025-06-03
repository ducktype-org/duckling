#pragma once

#include "../lists/attribute_arg_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Attribute element, can be before any statement.
	 */
	class Attribute final: public NotStmt {
		AccessInternal<DottedName> name;
		AccessInternal<AtrArgList> args;

	public:
		explicit Attribute(dia::SourcePosition& pos): NotStmt(pos) {}

		static MBox<Attribute> parse(LangParserState& state);
		~Attribute() final = default;

		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
		}
	};
}
