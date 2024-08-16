/**
 * @note This file is currently not compiled and not linter, because the corrsponding method
 * declarations were deleted for merging purposes. It might be added back in some different way.
 */

// NOLINTBEGIN

#include <base/variant.hpp>

#include "../elements/elements.hpp"
#include "lsp_elements.hpp"

using tpc::makeRef;

/**
 * @brief This file contains implementations of translators from PST nodes to LSPTree nodes.
 * For each new class, a pair of constructors has to be written.
 *
 * For example, for DottedName:
 * ParserRef<lsp::LSPDottedName> DottedName::dottedNameFromPST()
 * and
 * ParserRef<lsp::LSPNotStmt> DottedName::notStmtFromPST()
 *
 * As DottedName inherits from NotStmt, we have to create constructors for instances of
 * ParserRef<lsp::LSPDottedName> and ParserRef<lsp::LSPNotStmt>.
 */

namespace pst {

	namespace {
		void fillAction(
			lsp::LSPAction&                        action,
			const dia::SourcePosition&             position,
			const base::Optional<ParserRef<Expr>>& expr
		) {
			action.position = position;
			action.kind     = lsp::StmtKind::Action;
			if (expr.has_value()) action.expr = expr.value()->exprFromPST();
		}
	}

	ParserRef<lsp::LSPDottedName> DottedName::dottedNameFromPST() const {
		auto& res = *new lsp::LSPDottedName();
		res.star  = this->star;
		for (auto& name: this->names) res.names.push_back(name);
		return makeRef<lsp::LSPDottedName>(&res);
	}

	ParserRef<lsp::LSPNotStmt> DottedName::notStmtFromPST() const { return dottedNameFromPST(); }

	ParserRef<lsp::LSPStmt> Stmt::stmtFromPST() const {
		throw(std::runtime_error("stmtFromPST not implemented"));
	}

	ParserRef<lsp::LSPNotStmt> NotStmt::notStmtFromPST() const {
		throw(std::runtime_error("notStmtFromPST not implemented"));
	}

	ParserRef<lsp::LSPAttribute> Attribute::attributeFromPST() const {
		lsp::LSPAttribute& res = *new lsp::LSPAttribute;
		res.position           = this->getSourcePosition();
		res.name               = this->name;
		if (this->args)
			for (const auto& arg: this->args) res.args->elements.emplace_back(arg->exprFromPST());
		res.kind = lsp::StmtKind::Attribute;
		return makeRef<lsp::LSPAttribute>(&res);
	}

	ParserRef<lsp::LSPStmt> Attribute::stmtFromPST() const { return attributeFromPST(); }

	ParserRef<lsp::LSPImport> Import::importFromPST() const {
		lsp::LSPImport& res = *new lsp::LSPImport;
		res.position        = this->getSourcePosition();
		res.alias           = this->alias;
		if (this->names) res.names = this->names->dottedNameFromPST();
		res.kind = lsp::StmtKind::Import;
		return makeRef<lsp::LSPImport>(&res);
	}

	ParserRef<lsp::LSPStmt> Import::stmtFromPST() const { return importFromPST(); }

	ParserRef<lsp::LSPUsing> Using::usingFromPST() const {
		lsp::LSPUsing& res = *new lsp::LSPUsing;
		res.position       = this->getSourcePosition();
		if (this->names) res.names = this->names->dottedNameFromPST();
		res.kind = lsp::StmtKind::Using;
		return makeRef<lsp::LSPUsing>(&res);
	}

	ParserRef<lsp::LSPStmt> Using::stmtFromPST() const { return usingFromPST(); }

	ParserRef<lsp::LSPAlias> Alias::aliasFromPST() const {
		lsp::LSPAlias& res = *new lsp::LSPAlias;
		res.position       = this->getSourcePosition();
		res.name           = this->name;
		if (this->points_to) res.points_to = this->points_to->dottedNameFromPST();
		res.kind = lsp::StmtKind::Alias;
		return makeRef<lsp::LSPAlias>(&res);
	}

	ParserRef<lsp::LSPStmt> Alias::stmtFromPST() const { return aliasFromPST(); }

	ParserRef<lsp::LSPCodeBlock> CodeBlock::codeBlockFromPST() const {
		lsp::LSPCodeBlock& res = *new lsp::LSPCodeBlock;
		res.position           = this->getSourcePosition();
		for (auto& stmt: this->statements)
			if (stmt) res.statements.push_back(stmt->stmtFromPST());
		return makeRef<lsp::LSPCodeBlock>(&res);
	}

	ParserRef<lsp::LSPNotStmt> CodeBlock::notStmtFromPST() const { return codeBlockFromPST(); }

	ParserRef<lsp::LSPCodeBlockOrStmt> CodeBlockOrStmt::codeBlockOrStmtFromPST() const {
		lsp::LSPCodeBlockOrStmt& res = *new lsp::LSPCodeBlockOrStmt;
		res.position                 = this->getSourcePosition();
		variant_match(content) {
			variant_case(ParserRef<Stmt>, stmt) {
				if (stmt) res.content = stmt->stmtFromPST();
			}
			variant_case(ParserRef<CodeBlock>, codeBlock) {
				if (codeBlock) res.content = codeBlock->codeBlockFromPST();
			}
		}
		return makeRef<lsp::LSPCodeBlockOrStmt>(&res);
	}

	ParserRef<lsp::LSPNotStmt> CodeBlockOrStmt::notStmtFromPST() const {
		return codeBlockOrStmtFromPST();
	}

	ParserRef<lsp::LSPRoundGroupExpr> RoundGroupExpr::roundGroupExprFromPST() const {
		lsp::LSPRoundGroupExpr& res = *new lsp::LSPRoundGroupExpr;
		res.position                = this->getSourcePosition();
		if (this->expr) res.expr = this->expr->exprFromPST();
		return makeRef<lsp::LSPRoundGroupExpr>(&res);
	}

	ParserRef<lsp::LSPNotStmt> RoundGroupExpr::notStmtFromPST() const {
		return roundGroupExprFromPST();
	}

	ParserRef<lsp::LSPExpr> Expr::exprFromPST() const {
		lsp::LSPExpr& res = *new lsp::LSPExpr();
		for (auto& elem: this->elements) {
			variant_match(elem) {
				variant_case(Operator, op) {
					res.elements.emplace_back(lsp::LSPExpr::Operator{ op.oper_id, op.position });
				}
				variant_case(Group, g) {
					res.elements.emplace_back(lsp::LSPExpr::Group{
						(lsp::LSPExpr::GroupType)(int) (g.type),
						g.expr ? g.expr->exprFromPST() : nullptr,
						g.position });
				}
				variant_case(Identifier, id) {
					res.elements.emplace_back(lsp::LSPExpr::Identifier{ id.indent_id, id.position }
					);
				}
				variant_case(NumLiteral, num) {
					res.elements.emplace_back(lsp::LSPExpr::NumLiteral{ num.num_id, num.position });
				}
				variant_case(KeywordValue, kw) {
					res.elements.emplace_back(lsp::LSPExpr::KeywordValue{ kw.keyword, kw.position }
					);
				}
				variant_default { throw(std::runtime_error("Unknown ExprElem variant")); }
			}
		}
		res.kind = lsp::StmtKind::Expr;
		return makeRef<lsp::LSPExpr>(&res);
	}

	ParserRef<lsp::LSPStmt> Expr::stmtFromPST() const { return exprFromPST(); }

	ParserRef<lsp::LSPAction> Action::actionFromPST() const {
		lsp::LSPAction& res = *new lsp::LSPAction;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPAction>(&res);
	}

	ParserRef<lsp::LSPStmt> Action::stmtFromPST() const { return actionFromPST(); }

	ParserRef<lsp::LSPReturn> Return::returnFromPST() const {
		lsp::LSPReturn& res = *new lsp::LSPReturn;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPReturn>(&res);
	}

	ParserRef<lsp::LSPAction> Return::actionFromPST() const { return returnFromPST(); }

	ParserRef<lsp::LSPBreak> Break::breakFromPST() const {
		lsp::LSPBreak& res = *new lsp::LSPBreak;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPBreak>(&res);
	}

	ParserRef<lsp::LSPAction> Break::actionFromPST() const { return breakFromPST(); }

	ParserRef<lsp::LSPContinue> Continue::continueFromPST() const {
		lsp::LSPContinue& res = *new lsp::LSPContinue;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPContinue>(&res);
	}

	ParserRef<lsp::LSPAction> Continue::actionFromPST() const { return continueFromPST(); }

	ParserRef<lsp::LSPRedo> Redo::redoFromPST() const {
		lsp::LSPRedo& res = *new lsp::LSPRedo;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPRedo>(&res);
	}

	ParserRef<lsp::LSPAction> Redo::actionFromPST() const { return redoFromPST(); }

	ParserRef<lsp::LSPRestart> Restart::restartFromPST() const {
		lsp::LSPRestart& res = *new lsp::LSPRestart;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPRestart>(&res);
	}

	ParserRef<lsp::LSPAction> Restart::actionFromPST() const { return restartFromPST(); }

	ParserRef<lsp::LSPDefer> Defer::deferFromPST() const {
		lsp::LSPDefer& res = *new lsp::LSPDefer;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPDefer>(&res);
	}

	ParserRef<lsp::LSPAction> Defer::actionFromPST() const { return deferFromPST(); }

	ParserRef<lsp::LSPThrow> Throw::throwFromPST() const {
		lsp::LSPThrow& res = *new lsp::LSPThrow;
		fillAction(res, this->getSourcePosition(), this->expr);
		return makeRef<lsp::LSPThrow>(&res);
	}

	ParserRef<lsp::LSPAction> Throw::actionFromPST() const { return throwFromPST(); }

	ParserRef<lsp::LSPConst> Const::constFromPST() const {
		lsp::LSPConst& res = *new lsp::LSPConst;
		res.position       = this->getSourcePosition();
		res.name           = this->name;
		if (this->type) res.type = this->type->exprFromPST();
		if (this->value) res.value = this->value->exprFromPST();
		res.kind = lsp::StmtKind::Const;
		return makeRef<lsp::LSPConst>(&res);
	}

	ParserRef<lsp::LSPStmt> Const::stmtFromPST() const { return constFromPST(); }

	ParserRef<lsp::LSPDecl> Decl::declFromPST() const {
		lsp::LSPDecl& res = *new lsp::LSPDecl;
		res.position      = this->getSourcePosition();
		res.kind          = lsp::StmtKind::Decl;
		return makeRef<lsp::LSPDecl>(&res);
	}

	ParserRef<lsp::LSPStmt> Decl::stmtFromPST() const { return declFromPST(); }

	ParserRef<lsp::LSPCodeDecl> CodeDecl::codeDeclFromPST() const {
		lsp::LSPCodeDecl& res = *new lsp::LSPCodeDecl;
		res.position          = this->getSourcePosition();
		res.kind              = lsp::StmtKind::CodeDecl;
		return makeRef<lsp::LSPCodeDecl>(&res);
	}

	ParserRef<lsp::LSPDecl> CodeDecl::declFromPST() const { return codeDeclFromPST(); }

	ParserRef<lsp::LSPTopLevel> TopLevel::topLevelFromPST() const {
		lsp::LSPTopLevel& res = *new lsp::LSPTopLevel;
		res.position          = this->getSourcePosition();
		res.kind              = lsp::StmtKind::TopLevel;
		for (auto& stmt: this->statements)
			if (stmt) res.statements.push_back(stmt->stmtFromPST());
		return makeRef<lsp::LSPTopLevel>(&res);
	}

	ParserRef<lsp::LSPDecl> TopLevel::declFromPST() const { return topLevelFromPST(); }

	ParserRef<lsp::LSPBlock> Block::blockFromPST() const {
		lsp::LSPBlock& res = *new lsp::LSPBlock;
		res.position       = this->getSourcePosition();
		res.optional_name  = this->optional_name;
		if (this->code_block) res.code_block = this->code_block->codeBlockFromPST();
		res.kind = lsp::StmtKind::CodeDecl;
		return makeRef<lsp::LSPBlock>(&res);
	}

	ParserRef<lsp::LSPCodeDecl> Block::codeDeclFromPST() const { return blockFromPST(); }

	ParserRef<lsp::LSPNamespace> Namespace::namespaceFromPST() const {
		lsp::LSPNamespace& res = *new lsp::LSPNamespace;
		res.position           = this->getSourcePosition();
		res.name               = this->name;
		if (this->body) res.body = this->body->codeBlockFromPST();
		res.kind = lsp::StmtKind::Namespace;
		return makeRef<lsp::LSPNamespace>(&res);
	}

	ParserRef<lsp::LSPDecl> Namespace::declFromPST() const { return namespaceFromPST(); }

	ParserRef<lsp::LSPStruct> Struct::structFromPST() const {
		lsp::LSPStruct& res = *new lsp::LSPStruct;
		res.position        = this->getSourcePosition();
		res.name            = this->name;
		if (this->bases)
			for (const auto& base: this->bases) res.bases.push_back(base->exprFromPST());
		if (this->body) res.body = this->body->codeBlockFromPST();
		res.kind = lsp::StmtKind::Struct;
		return makeRef<lsp::LSPStruct>(&res);
	}

	ParserRef<lsp::LSPDecl> Struct::declFromPST() const { return structFromPST(); }

	ParserRef<lsp::LSPFun> Fun::funFromPST() const {
		auto* res     = new lsp::LSPFun;
		res->position = this->getSourcePosition();
		res->name     = this->name;
		if (this->params) {
			auto& resParams = *new lsp::LSPList<lsp::LSPExpr>;
			res->params     = makeRef<lsp::LSPList<lsp::LSPExpr>>(&resParams);
			for (const auto& param: this->params)
				res->params->elements.emplace_back(param->exprFromPST());
		}
		if (this->rets) {
			auto& resRets = *new lsp::LSPList<lsp::LSPExpr>;
			res->rets     = makeRef<lsp::LSPList<lsp::LSPExpr>>(&resRets);
			for (const auto& ret: this->rets) res->rets->elements.emplace_back(ret->exprFromPST());
		}
		if (this->body) res->body = this->body->codeBlockOrStmtFromPST();
		res->kind = lsp::StmtKind::Fun;
		return makeRef<lsp::LSPFun>(res);
	}

	ParserRef<lsp::LSPDecl> Fun::declFromPST() const { return funFromPST(); }

	ParserRef<lsp::LSPVariable> Variable::variableFromPST() const {
		auto* res     = new lsp::LSPVariable;
		res->position = this->getSourcePosition();
		res->name     = this->name;
		if (this->type) res->type = this->type->exprFromPST();
		if (this->value) res->value = this->value->exprFromPST();
		res->is_const = this->is_const;
		res->kind     = lsp::StmtKind::Variable;
		return makeRef<lsp::LSPVariable>(res);
	}

	ParserRef<lsp::LSPDecl> Variable::declFromPST() const { return variableFromPST(); }

	ParserRef<lsp::LSPIf> If::ifFromPST() const {
		lsp::LSPIf& res = *new lsp::LSPIf;
		res.position    = this->getSourcePosition();
		if (this->condition) res.condition = this->condition->roundGroupExprFromPST();
		res.optional_name = this->optional_name;
		if (this->body) res.body = this->body->codeBlockOrStmtFromPST();
		res.kind = lsp::StmtKind::CodeDecl;
		return makeRef<lsp::LSPIf>(&res);
	}

	ParserRef<lsp::LSPCodeDecl> If::codeDeclFromPST() const { return ifFromPST(); }

	ParserRef<lsp::LSPWhile> While::whileFromPST() const {
		lsp::LSPWhile& res = *new lsp::LSPWhile;
		res.position       = this->getSourcePosition();
		if (this->condition) res.condition = this->condition->roundGroupExprFromPST();
		res.optional_name = this->optional_name;
		if (this->body) res.body = this->body->codeBlockOrStmtFromPST();
		res.kind = lsp::StmtKind::CodeDecl;
		return makeRef<lsp::LSPWhile>(&res);
	}

	ParserRef<lsp::LSPCodeDecl> While::codeDeclFromPST() const { return whileFromPST(); }

}

// NOLINTEND
