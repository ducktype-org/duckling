import { NotStmt, Stmt, stmtFactory, notStmtFactory, NotStmtFactory } from "./elements";
import { ElementFactory } from "./element_factory";

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

	getElements() {
		return this.statements;
	}

	getSemanticTokens() {
		return this.statements.flatMap(stmt => stmt.getSemanticTokens());
	}
}

export type CodeBlockFactory = ElementFactory<CodeBlock, NotStmtFactory>;
export const codeBlockFactory = new ElementFactory<CodeBlock, NotStmtFactory>(notStmtFactory);

codeBlockFactory.register("CodeBlock", CodeBlock);