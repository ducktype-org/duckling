import { CodeBlock } from "./code_block";
import { NotStmt, Stmt, stmtFactory, NotStmtFactory, notStmtFactory, DucklingElement } from "./elements";
import { ElementException, ElementFactory } from "./element_factory";
import { codeBlockFactory } from "./code_block";

export class CodeBlockOrStmt extends NotStmt {
	value: CodeBlock | Stmt;

	constructor(json: any) {
		super(json);
		const content = json["value"];
		if (codeBlockFactory.isObjectClassValid(content)) {
			this.value = codeBlockFactory.createDefined(content);
		} else if (stmtFactory.isObjectClassValid(content)) {
			this.value = stmtFactory.createDefined(content);
		} else {
			throw new ElementException("Invalid value for CodeBlockOrStmt: " + content + " is not a valid CodeBlock or Stmt.");
		}
	}

	getElements(): DucklingElement[] {
		return [this.value];
	}

	getSemanticTokens() {
		return this.value.getSemanticTokens();
	}
}

export type CodeBlockOrStmtFactory = ElementFactory<CodeBlockOrStmt, NotStmtFactory>;
export const codeBlockOrStmtFactory = new ElementFactory<CodeBlockOrStmt, NotStmtFactory>(notStmtFactory);

codeBlockOrStmtFactory.register("CodeBlockOrStmt", CodeBlockOrStmt);