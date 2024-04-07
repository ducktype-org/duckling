import { ElementFactory, ParentlessElementFactory } from "./element_factory";
import { SourcePosition, SemanticToken } from "./common";

enum StmtType {
}


export abstract class RiftElement {
	source_position: SourcePosition;

	constructor(json: any) {
		this.source_position = new SourcePosition(json["position"]);
	}

	abstract getElements(): RiftElement[];
	abstract getSemanticTokens(): SemanticToken[];
}

export abstract class Stmt extends RiftElement {

}

interface StmtJSON {
	new(json: any): Stmt;
}

export abstract class Decl extends Stmt {

}

export abstract class NotStmt extends RiftElement {
}

export abstract class CodeDecl extends Decl {
}

export type RiftElementFactory = ParentlessElementFactory<RiftElement>;
export const riftElementFactory = new ParentlessElementFactory<RiftElement>();

export type StmtFactory = ElementFactory<Stmt, RiftElementFactory>;
export const stmtFactory = new ElementFactory<Stmt, RiftElementFactory>(riftElementFactory);

export type DeclFactory = ElementFactory<Decl, StmtFactory>;
export const declFactory = new ElementFactory<Decl, StmtFactory>(stmtFactory);

export type NotStmtFactory = ElementFactory<NotStmt, RiftElementFactory>;
export const notStmtFactory = new ElementFactory<NotStmt, RiftElementFactory>(riftElementFactory);

export type CodeDeclFactory = ElementFactory<CodeDecl, DeclFactory>;
export const codeDeclFactory = new ElementFactory<CodeDecl, DeclFactory>(declFactory);