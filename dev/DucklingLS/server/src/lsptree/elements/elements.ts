import { ElementFactory, ParentlessElementFactory } from "./element_factory";
import { SourcePosition, SemanticToken } from "./common";
import { FoldingRange } from "vscode-languageserver";

export abstract class DucklingElement {
	source_position: SourcePosition;

	constructor(json: any) {
		this.source_position = new SourcePosition(json["position"]);
	}

	// Function used in the implementation of `code completion`.
	getIdentifiers(): DucklingElement[] {
		let acc = new Set<DucklingElement>();
		for (const element of this.getElements()) {
			element.getIdentifiers().forEach((element1) => acc.add(element1));
		}
		return [...acc.values()];
	}
	
	// A universal function used to write tree crawlers.
	abstract getElements(): DucklingElement[];
	// Function used in the implementation of `syntax highlighting`.
	abstract getSemanticTokens(): SemanticToken[];
	// Function used in the implementation of `folding ranges`.
	getFoldingRanges(): FoldingRange[] {
		return this.getElements().flatMap(element => element.getFoldingRanges());
	}
}

export abstract class Stmt extends DucklingElement {
}

export abstract class Decl extends Stmt {
}

export abstract class NotStmt extends DucklingElement {
}

export abstract class CodeDecl extends Decl {
}

export type DucklingElementFactory = ParentlessElementFactory<DucklingElement>;
export const ducklingElementFactory = new ParentlessElementFactory<DucklingElement>();

export type StmtFactory = ElementFactory<Stmt, DucklingElementFactory>;
export const stmtFactory = new ElementFactory<Stmt, DucklingElementFactory>(ducklingElementFactory);

export type DeclFactory = ElementFactory<Decl, StmtFactory>;
export const declFactory = new ElementFactory<Decl, StmtFactory>(stmtFactory);

export type NotStmtFactory = ElementFactory<NotStmt, DucklingElementFactory>;
export const notStmtFactory = new ElementFactory<NotStmt, DucklingElementFactory>(ducklingElementFactory);

export type CodeDeclFactory = ElementFactory<CodeDecl, DeclFactory>;
export const codeDeclFactory = new ElementFactory<CodeDecl, DeclFactory>(declFactory);
