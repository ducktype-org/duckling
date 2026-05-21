#pragma once

#include "preamble.hpp"

namespace pst {
	class Namespace final: public Decl {
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(body, CodeBlock);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Namespace, ElementKind::Namespace);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
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

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
