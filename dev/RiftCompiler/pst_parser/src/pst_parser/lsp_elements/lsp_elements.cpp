#include <token_parser_core/parser_ref.hpp>

#include "lsp_elements.hpp"
#include "../../../../../base/src/base/ints.hpp"

namespace lsp {

	template<class T>
	void nullAwareLspPrint(const tpc::ParserRef<T>& ref, std::ostream& out) {
		if (ref)
			ref->lsp_print(out);
		else
			out << "\"<nullptr>\"";
	}

	void positionPrint(std::ostream& out, const dia::SourcePosition& pos) {
		std::pair<usize, usize> startLineColumn = pos.getStartLineColumn();
		std::pair<usize, usize> endLineColumn   = pos.getEndLineColumn();


		out << "\"position\": {";
		out << "\"startLine\": " << startLineColumn.first;
		out << ",\"startColumn\": " << startLineColumn.second;
		out << ",\"endLine\": " << endLineColumn.first;
		out << ",\"endColumn\": " << endLineColumn.second;
		out << ",\"start\": " << pos.getStart();
		out << ",\"end\": " << pos.getEnd();
		out << "}";
	}

	void lspIdentifierPrint(base::StrId value, const dia::SourcePosition& pos, std::ostream& out) {
		// @TODO: change Name to Identifier
		out << "{\"Name\":";
		out << "{\"value\":";
		if (value.isBad())
			out << "\"BAD_NAME\"";
		else
			out << "\"" << value.strView() << "\"";
		out << ",";
		positionPrint(out, pos);
		out << "}}";
	}

	void nullAwareLspPrint(const tpc::Identifier& ident, std::ostream& out) {
		lspIdentifierPrint(ident.value, ident.position, out);
	}

	void nullAwareLspPrint(tpc::OptionalIdentifier ident, std::ostream& out) {
		if (ident.value.has_value())
			lspIdentifierPrint(ident.value.value(), ident.position, out);
		else
			out << "\"<ANONYMOUS>\"";
	}

	void LSPElement::lsp_print(std::ostream& out) {
		out << "LSPElement";
		// This should technically never be printed
	}

	void lspOptionalIdentifierPrint(tpc::OptionalIdentifier ident, std::ostream& out) {
		if (ident.value.has_value())
			lspIdentifierPrint(ident.value.value(), ident.position, out);
		else
			out << "\"<ANONYMOUS>\"";
	}

	void lsp_print_keyword(std::ostream& out, tpc::Keyword keyword) {
		out << "\"" << rift_def::keywordToStr(keyword).view().stringView() << "\"";
	}

	void lsp_print_strId(std::ostream& out, base::StrId id) {
		if (id.isBad())
			out << "\"BAD_NAME\"";
		else
			out << "\"" << id.strView() << "\"";
	}

	void LSPDottedName::lsp_print(std::ostream& out) {
		out << "{\"DottedName\": {";
		out << "\"names\": [";
		for (auto& x: names) {
			nullAwareLspPrint(x, out);
			if (&x != &names.back()) out << ",";
		}
		out << "],";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPStmt::lsp_print(std::ostream& out) {
		out << "{\"LSPStmt\" : {";
		out << "\"position\": ";
		positionPrint(out, position);
		out << "}";
		// out << "\"kind\" : " + std::to_string(kind) + "}";
	}  // Likewise, this should never be printed

	void LSPNotStmt::lsp_print(std::ostream& out) {
		out << "{LSPNotStmt :{}}";
		throw std::runtime_error("LSPNotStmt should never be printed");
	}  // This should never be printed

	static_assertTrue(std::is_base_of_v<LSPElement, LSPExpr>);

	void LSPAttribute::lsp_print(std::ostream& out) {
		out << "{\"Attribute\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"value\":";
		nullAwareLspPrint(args, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPImport::lsp_print(std::ostream& out) {
		out << "{\"Import\": {";
		out << "\"alias\":";
		nullAwareLspPrint(alias, out);
		out << ",\"names\":";
		nullAwareLspPrint(names, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPUsing::lsp_print(std::ostream& out) {
		out << "{\"Using\": {";
		out << "\"names\":";
		nullAwareLspPrint(names, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPAlias::lsp_print(std::ostream& out) {
		out << "{\"Alias\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"point_to\":";
		nullAwareLspPrint(points_to, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPCodeBlock::lsp_print(std::ostream& out) {
		out << "{\"CodeBlock\": {";

		out << "\"value\": [";
		for (auto& stmt: statements) {
			nullAwareLspPrint(stmt, out);
			if (&stmt != &statements.back()) out << ",";
		}
		out << "],";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPCodeBlockOrStmt::lsp_print(std::ostream& out) {
		out << "{\"CodeBlockOrStmt\": {";
		out << "\"value\": ";
		VISIT(content, inner, inner->lsp_print(out));
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPRoundGroupExpr::lsp_print(std::ostream& out) {
		out << "{\"RoundGroupExpr\": {";
		out << "\"expr\":";
		nullAwareLspPrint(expr, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::lsp_print(std::ostream& out) {
		out << "{\"Expr\": {";
		out << "\"value\": [";
		for (auto& element: elements) {
			VISIT(element, inner, inner.lsp_print(out));
			if (&element != &elements.back()) out << ",";
		}
		out << "],";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::Group::lsp_print(std::ostream& out) {
		out << "{\"Group\": {";
		out << "\"type\": ";
		switch (type) {
		case GroupType::RoundGroup:
			out << "\"()\",";
			break;
		case GroupType::SquareGroup:
			out << "\"[]\",";
			break;
		case GroupType::CurlyGroup:
			out << "\"{}\",";
			break;
		case GroupType::AngleGroup:
			out << "\"<>\",";
			break;
		}
		out << "\"expr\": ";
		nullAwareLspPrint(expr, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::Operator::lsp_print(std::ostream& out) {
		out << "{\"Operator\": {";
		out << "\"oper_id\": ";
		lsp_print_strId(out, oper_id);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::Identifier::lsp_print(std::ostream& out) {
		out << "{\"Identifier\": {";
		out << "\"indent_id\": ";
		lsp_print_strId(out, indent_id);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::NumLiteral::lsp_print(std::ostream& out) {
		out << "{\"NumLiteral\": {";
		out << "\"num_id\": ";
		lsp_print_strId(out, num_id);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPExpr::KeywordValue::lsp_print(std::ostream& out) {
		out << "{\"KeywordValue\": {";
		out << "\"keyword\": ";
		lsp_print_keyword(out, keyword);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPAction::lsp_print(std::ostream& out) {
		out << "Should never print";
		throw(std::runtime_error("Raw LSPAction should never be printed"));
	}

	void LSPAction::parametrizedLSPPrint(
		std::ostream&                            out,
		const std::optional<ParserRef<LSPExpr>>& action,
		const std::string_view                   name,
		const std::string&                       preposition
	) const {
		out << "{\"" << name << "\" : {";
		if (action) {
			out << "\"" << preposition << "\": ";
			nullAwareLspPrint(action.value(), out);
			out << ",";
			positionPrint(out, position);
		} else {
			positionPrint(out, position);
		}
		out << "}}";
	}

	void LSPReturn::lsp_print(std::ostream& out) {
		out << "{\"Return\": {";
		out << "\"expr\": ";
		parametrizedLSPPrint(out, expr, "Return", "with");
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPBreak::lsp_print(std::ostream& out) {
		parametrizedLSPPrint(out, expr, "Break", "from");
	}

	void LSPContinue::lsp_print(std::ostream& out) {
		parametrizedLSPPrint(out, expr, "Continue", "with");
	}

	void LSPRedo::lsp_print(std::ostream& out) { parametrizedLSPPrint(out, expr, "Redo", "what"); }

	void LSPRestart::lsp_print(std::ostream& out) {
		parametrizedLSPPrint(out, expr, "Restart", "what");
	}

	void LSPDefer::lsp_print(std::ostream& out) {
		parametrizedLSPPrint(out, expr, "Defer", "what");
	}

	void LSPThrow::lsp_print(std::ostream& out) {
		parametrizedLSPPrint(out, expr, "Throw", "exception");
	}

	void LSPConst::lsp_print(std::ostream& out) {
		out << "{\"Const\": {";
		out << "\"name\": ";
		nullAwareLspPrint(name, out);
		out << ",\"type\": ";
		nullAwareLspPrint(type, out);
		out << ",\"value\": ";
		nullAwareLspPrint(value, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPDecl::lsp_print(std::ostream& out) {
		out << "Should never print";
		throw(std::runtime_error("Raw LSPDecl should never be printed"));
	}

	void LSPCodeDecl::lsp_print(std::ostream& out) {
		out << "Should never print";
		throw(std::runtime_error("Raw LSPCodeDecl should never be printed"));
	}

	void LSPTopLevel::lsp_print(std::ostream& out) {
		out << "{\"LSPTree\": {";
		out << "\"value\": [";
		for (auto& stmt: statements) {
			nullAwareLspPrint(stmt, out);
			if (&stmt != &statements.back()) out << ",";
		}
		out << "],";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPNamespace::lsp_print(std::ostream& out) {
		out << "{\"Namespace\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"body\":";
		nullAwareLspPrint(body, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPStruct::lsp_print(std::ostream& out) {
		out << "{\"Struct\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"bases\": [";
		for (auto& base: bases) {
			nullAwareLspPrint(base, out);
			if (&base != &bases.back()) out << ",";
		}
		out << "],\"body\":";
		nullAwareLspPrint(body, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPFun::lsp_print(std::ostream& out) {
		out << "{\"Fun\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"params\":";
		nullAwareLspPrint(params, out);
		out << ",\"rets\":";
		nullAwareLspPrint(rets, out);
		out << ",\"body\":";
		nullAwareLspPrint(body, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPVariable::lsp_print(std::ostream& out) {
		out << "{\"Variable\": {";
		out << "\"name\":";
		nullAwareLspPrint(name, out);
		out << ",\"type\":";
		nullAwareLspPrint(type, out);
		out << ",\"value\":";
		nullAwareLspPrint(value, out);
		out << ",\"is_const\":";
		out << is_const;
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPBlock::lsp_print(std::ostream& out) {
		out << "{\"Block\": {";
		out << "\"name\":";
		nullAwareLspPrint(optional_name, out);
		out << ",\"code_block\":";
		nullAwareLspPrint(code_block, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPIf::lsp_print(std::ostream& out) {
		out << "{\"If\": {";
		out << "\"condition\":";
		nullAwareLspPrint(condition, out);
		out << ",\"name\":";
		nullAwareLspPrint(optional_name, out);
		out << ",\"body\":";
		nullAwareLspPrint(body, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	void LSPWhile::lsp_print(std::ostream& out) {
		out << "{\"While\": {";
		out << "\"condition\":";
		nullAwareLspPrint(condition, out);
		out << ",\"name\":";
		nullAwareLspPrint(optional_name, out);
		out << ",\"body\":";
		nullAwareLspPrint(body, out);
		out << ",";
		positionPrint(out, position);
		out << "}}";
	}

	template<LSPElementSubclass T>
	void LSPList<T>::lsp_list_parametrized_print(std::ostream& out, const std::string& name) {
		out << "{\"" << name << "\": {";
		out << "\"elements\": [";
		for (auto& element: elements) {
			nullAwareLspPrint(element, out);
			if (&element != &elements.back()) out << ",";
		}
		out << "],";
		positionPrint(out, position);
		out << "}}";
	}

	template<>
	void LSPList<LSPExpr>::lsp_print(std::ostream& out) {
		lsp_list_parametrized_print(out, "ExprList");
	}
}
