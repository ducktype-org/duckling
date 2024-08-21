import { NotStmt, Stmt, stmtFactory, notStmtFactory, NotStmtFactory, DucklingElement } from "./elements";
import { ElementFactory } from "./element_factory";
import { FoldingRange, FoldingRangeKind } from "vscode-languageserver";

export class CodeBlock extends NotStmt {
	statements: Stmt[];


	constructor(json: any) {
		super(json);
		this.statements = [];
		for (const stmt_params of json["value"]) {
			const stmt = stmtFactory.create(stmt_params);
			if (stmt) this.statements.push(stmt);
		}
	}

	getElements(): DucklingElement[] {
		return this.statements;
	}

	getSemanticTokens() {
		return this.statements.flatMap(stmt => stmt.getSemanticTokens());
	}

	
	getFoldingRanges(): FoldingRange[] {
		if (this.statements.length === 0) {
			return [];
		}
		
		const lastStmt = this.statements[this.statements.length - 1];

		const blockFoldingRange: FoldingRange = {
			startLine: this.source_position.startLine - 1, // because the folding range is 0-indexed
			startCharacter: this.source_position.startColumn,
			endLine: this.source_position.endLine - 1, // same here
			endCharacter: this.source_position.endColumn,
			kind: FoldingRangeKind.Region,
		};

		const childFoldingRanges = this.statements.flatMap(stmt => stmt.getFoldingRanges());

		return [blockFoldingRange, ...childFoldingRanges];
	}
}

export type CodeBlockFactory = ElementFactory<CodeBlock, NotStmtFactory>;
export const codeBlockFactory = new ElementFactory<CodeBlock, NotStmtFactory>(notStmtFactory);

codeBlockFactory.register("CodeBlock", CodeBlock);