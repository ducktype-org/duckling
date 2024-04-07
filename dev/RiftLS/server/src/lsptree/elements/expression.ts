import { Identifier } from "./identifier";
import { RiftElement, Stmt, stmtFactory,riftElementFactory, RiftElementFactory, StmtFactory } from "./elements";
import { ElementFactory } from "./element_factory";
import { SemanticToken } from "./common";


export abstract class ExprElem extends RiftElement {

}

export type ExprElemFactory = ElementFactory<ExprElem, RiftElementFactory>;
export const exprElemFactory = new ElementFactory<ExprElem, RiftElementFactory>(riftElementFactory);

export class Expr extends Stmt {
    
	elements: ExprElem[];

	constructor(json: any) {
		super(json);
		this.elements = [];
		for (const elem_params of json["value"]) {
			const elem = exprElemFactory.create(elem_params);
			if (elem) this.elements.push(elem);
		}
	}

	getElements() {
		return this.elements;
	}

	getSemanticTokens() {
		return this.elements.flatMap(elem => elem.getSemanticTokens());
	}
}

export type ExprFactory = ElementFactory<Expr, StmtFactory>;
export const exprFactory = new ElementFactory<Expr, StmtFactory>(stmtFactory);

exprFactory.register("Expr", Expr);


export class Group extends ExprElem {
	expr?: Expr;

	constructor(json: any) {
		super(json);
		this.expr = exprFactory.create(json["expr"]);
	}
	
	getElements(): RiftElement[] {
		return this.expr?.getElements() ?? [];
	}

	getSemanticTokens(): SemanticToken[] {
		return this.expr?.getSemanticTokens() ?? [];
	}
}

exprElemFactory.register("Group", Group);
