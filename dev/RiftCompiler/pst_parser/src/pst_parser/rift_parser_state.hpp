#pragma once

#include <algorithm>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/automatic.hpp>
#include <base/strongly_typed_id.hpp>
#include <utility>

#include "rift_parser_element.hpp"
#include "pst_automatic.hpp"

namespace pst {
	class RiftParserState final: public tpc::ParserState {
		std::vector<ImportType> imports;

	public:
		RiftParserState(tpc::TokenStream&& tokens, dia::Logger& err):
			  tpc::ParserState(std::move(tokens), err) {}

		void addImport(const tpc::ParserCBorrowRef<pst::Import>& import);

		[[nodiscard]]
		auto extractState() && -> std::vector<ImportType> {
			return std::move(imports);
		}

		template<std::derived_from<RiftElement> El>
		pst::PSTAutomatic<RiftParserState> parse(ParserRef<El>& el) {
			return { *this, el.borrow_mut() };
		}

		template<std::derived_from<RiftElement> El>
		pst::PSTAutomatic<RiftParserState> parse(ParserBorrowRef<El>& el) {
			return { *this, el };
		}
	};

}
