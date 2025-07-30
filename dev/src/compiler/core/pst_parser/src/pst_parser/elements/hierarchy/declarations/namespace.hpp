#pragma once

#include "preamble.hpp"

namespace pst {
	class Namespace final: public Decl {
		tpc::Identifier           name;
		NAMED_CHILD(body, CodeBlock);

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace, ElementKind::Namespace);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<CodeBlock> getBody() const {
			return body.give();
		}

		static MBox<Namespace> parse(LangParserState& state);
		~Namespace() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Namespace";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
