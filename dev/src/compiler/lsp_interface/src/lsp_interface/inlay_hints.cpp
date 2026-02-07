#include "inlay_hints.hpp"

#include "frontend/pst_parser/elements/hierarchy/declarations/const.hpp"
#include "frontend/pst_parser/elements/hierarchy/declarations/function.hpp"
#include "frontend/pst_parser/elements/hierarchy/declarations/variable.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp"
#include "helios/hout/hout.hpp"
#include "helios/queries.hpp"
#include "utils.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>

#include "diagnostic/source_position.hpp"
#include "query_framework/entry/query_entry_point.hpp"

namespace lsp {
	using namespace compiler;

	struct Position {
		u64 line;
		u64 character;
	};

	struct TextEdit {
		Position    start;
		Position    end;
		std::string new_text;

		std::string toLSPJson() const {
			return R"({"range": {"start": {"line": )" + std::to_string(start.line)
			     + ", \"character\": " + std::to_string(start.character) + "}, \"end\": {\"line\": "
			     + std::to_string(end.line) + ", \"character\": " + std::to_string(end.character)
			     + "}}, \"newText\": \"" + new_text + "\"}";
		}
	};

	struct InlayHint {
		std::string           label;
		Position              position;
		std::vector<TextEdit> text_edits;

		// ====== Not part of the protocol =======
		fs::FilePath source_file;

		[[nodiscard]] std::string toLSPJson() const {
			// With position
			std::string json = R"({"position": {"line": )" + std::to_string(position.line)
			                 + ", \"character\": " + std::to_string(position.character)
			                 + "}, \"label\": \"" + label + "\"";
			if (!text_edits.empty()) {
				json += ", \"textEdits\": [";
				for (size_t i = 0; i < text_edits.size(); i++)
					json += text_edits[i].toLSPJson() + (i < text_edits.size() - 1 ? "," : "");
				json += "]";
			}
			json += "}";
			return json;
		}

		static InlayHint fromPositionEnd(
			const std::string& label, dia::SourcePosition pos, bool is_text_edit = false
		) {
			auto [line, character] = pos.getEndLineColumn();
			Position position{ .line = line - 1, .character = character };
			if (is_text_edit) {
				TextEdit edit{ .start = position, .end = position, .new_text = label };
				return InlayHint{ .label       = label,
					              .position    = position,
					              .text_edits  = { edit },
					              .source_file = pos.getLocation()->getSourceFile().getFilePath() };
			} else {
				return InlayHint{ .label       = label,
					              .position    = position,
					              .text_edits  = {},
					              .source_file = pos.getLocation()->getSourceFile().getFilePath() };
			}
		}
	};

	void collectHintsFromVariable(
		pst::Access<pst::LangElement> pst_lang_element,
		tsh::SymbolType<>             type,
		std::vector<InlayHint>&       hints
	) {
		if (pst_lang_element.dynamicCast<pst::Variable>().empty()
		    and pst_lang_element.dynamicCast<pst::Const>().empty())
			return;

		bool has_initial_value = false;
		
		if_opt_some(pst_lang_element.dynamicCast<pst::Variable>(), var) {
			has_initial_value = var->getValue().has_value();
		}
		if_opt_some(pst_lang_element.dynamicCast<pst::Const>(), constant) {
			has_initial_value = constant->getValue().has_value();
		}
		if (not has_initial_value) return;

		tpc::Identifier variable_name = [](pst::Access<pst::LangElement> elem) {
			if_opt_some(elem.dynamicCast<pst::Variable>(), var) return var->getIdent();
			if_opt_some(elem.dynamicCast<pst::Const>(), constant) return constant->getIdent();

			CORE_PANIC("Expected variable or constant");
		}(pst_lang_element);

		InlayHint hint
			= InlayHint::fromPositionEnd(" : " + type.toString(), variable_name.position, true);
		hints.push_back(hint);
	}

	void collectHintsFromFunction(
		CRef<helios::HOUTFunction> function, std::vector<InlayHint>& hints
	) {
		auto pst_elements = function->origin.getPstElements();
		if (pst_elements.empty() || pst_elements.size() > 1) {
			// We expect exactly one PST element for the function declaration
			return;
		}
		auto pst_lang_element = pst_elements[0].illegalAccess().value();
		if (pst_lang_element.dynamicCast<pst::Fun>().empty()) return;

		auto function_pst = pst_lang_element.dynamicCast<pst::Fun>().value();

		// ==================== 1. Return type hint ====================
		if_opt_none(function_pst->getRet()) {
			auto hint = InlayHint::fromPositionEnd(
				" -> " + function->declaration->return_type.toString(),
				function_pst->getParams().illegalAccess().value()->getSourcePosition(),
				true
			);
			hints.push_back(std::move(hint));
		}

		// Now we have to go over stmt's
	}

	void collectHintsFromGlobalVariable(
		CRef<helios::HOUTGlobalData> global_variable, std::vector<InlayHint>& hints
	) {
		auto related_pst_elements = global_variable->origin.getPstElements();
		if (related_pst_elements.empty() || related_pst_elements.size() > 1) {
			// We expect exactly one PST element for the global variable declaration
			return;
		}
		auto pst_lang_element = related_pst_elements[0].illegalAccess().value();
		collectHintsFromVariable(pst_lang_element, global_variable->type, hints);
	}

	std::vector<InlayHint> collectHints(CRef<helios::HOUTUnit> hout_unit) {
		std::vector<InlayHint> hints;

		// Traverse the HOUT and collect inlay hints
		for (auto function: hout_unit->functions) collectHintsFromFunction(function, hints);
		for (auto& global_variable: hout_unit->glob_data)
			collectHintsFromGlobalVariable(&global_variable, hints);

		return hints;
	}

	std::string serializeHintsToJson(const std::vector<InlayHint>& hints) {
		std::string json = "[";
		for (size_t i = 0; i < hints.size(); i++) {
			json += hints[i].toLSPJson();
			if (i < hints.size() - 1) json += ",";
		}
		json += "]";
		return json;
	}

	std::string getInlayHintsJson(const fs::File& file) {
		auto source_files = frontend::SourceFile::getSourceFilesfromFile(file);
		CORE_ASSERT(
			not source_files.empty(),
			"File must be associated with at least one SourceFile in the ModuleTree"
		);

		auto module = source_files[source_files.size() - 1]->getModule().illegalAccess().getID();
		if (not isPackageParsedSuccessfully(module)) return "[]";

		// We should change this in the future to calculate manually per function from PST,
		// to keep the hints even if some element has compile errors.
		auto helios_module_result = query::entryPoint<helios::QueryModuleHOUT>(module);
		if (helios_module_result->hasFailed()) return "[]";

		auto helios_module = CRef<helios::HOUTUnit>(&helios_module_result->valueOrThrow());

		auto hints = collectHints(helios_module);

		auto file_hints = hints | std::views::filter([&file](const InlayHint& hint) {
							  return hint.source_file == file.getFilePath();
						  })
		                | std::ranges::to<std::vector>();

		return serializeHintsToJson(file_hints);
	}
}
