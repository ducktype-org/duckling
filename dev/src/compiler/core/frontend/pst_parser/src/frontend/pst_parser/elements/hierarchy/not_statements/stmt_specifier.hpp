#pragma once

#include "../meta.hpp"
#include "wrapper_elements/keyword_wrapper.hpp"

namespace pst {
	/**
	 * @brief A simple statement specifier (ex. public / private)
	 *
	 * Allows a limited set of keywords.
	 * Examples:
	 *  - public fun ...
	 *  - public { stmt1; stmt2; }
	 *  - extern ("C") {...}
..	 */
	class StmtSpecifier final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(StmtSpecifier, NotStmt);
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(specifier, KeywordWrapper);
		NAMED_CHILD_OPT(call_list, CallList);

	public:
		static constexpr std::array SPECIFIEIRS_CALL_LIST_REQUIRED_ARRAY = {
			Keyword::Extern,
		};

		static const std::set<Keyword> SPECIFIEIRS_CALL_LIST_REQUIRED;

		explicit StmtSpecifier(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::StmtSpecifier;
		}

		static MBox<StmtSpecifier> parse(LangParserState& state);

		~StmtSpecifier() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "StmtSpecifier";
		}

		[[nodiscard]]
		AccessLocked<KeywordWrapper> getSpecifier() const {
			return specifier.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<CallList>> getArgs() const {
			if (call_list.has_value())
				return call_list->give();
			else
				return {};
		}
	};
}
