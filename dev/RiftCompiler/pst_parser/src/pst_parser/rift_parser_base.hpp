#pragma once

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <base/strongly_typed_id.hpp>
#include <utility>

namespace pst {
	class Import;

	using tpc::ParserBorrowRef;
	using tpc::ParserCBorrowRef;
	using tpc::ParserRef;

	STRONG_TYPEDEF_ID(PstID);

	class RiftElement: public tpc::Element {
	public:
		explicit RiftElement(const dia::SourcePosition& position): source_position(position){};

		[[nodiscard]]
		const dia::SourcePosition& getSourcePosition() const;

		[[nodiscard]]
		const std::vector<dia::SourcePosition>& getKeywordPositions() const;

		[[nodiscard]]
		PstID getID() const {
			return id;
		}

	protected:
		dia::SourcePosition              source_position;
		std::vector<dia::SourcePosition> keyword_positions;

		void setLastToken(dia::SourcePosition pos);
		void addKeyword(dia::SourcePosition pos);

	private:
		PstID id = PstID::next();
	};

	using ImportType = tpc::ParserCBorrowRef<pst::Import>;

	class RiftParserState: public tpc::ParserState {
		std::vector<ImportType> imports;

	public:
		RiftParserState(tpc::TokenStream&& tokens, dia::Logger& err):
			  tpc::ParserState(std::move(tokens), err) {}

		void addImport(const tpc::ParserCBorrowRef<pst::Import>& import);

		[[nodiscard]]
		auto extractState() && -> std::vector<ImportType> {
			return std::move(imports);
		}
	};

}

ID_STD_HASH(::pst::PstID);
