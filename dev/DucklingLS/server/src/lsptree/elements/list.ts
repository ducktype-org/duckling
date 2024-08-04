import { NotStmt, notStmtFactory, NotStmtFactory, DucklingElement, ducklingElementFactory } from "./elements";
import { Expr } from "./expression";
import { ElementFactory } from "./element_factory";

export class List<T extends DucklingElement> extends NotStmt {
	elements: T[];

	constructor(json: any) {
		super(json);
		this.elements = [];
		for (const elem_params of json["elements"]) {
			const elem = ducklingElementFactory.create(elem_params);
			if (elem) 
				this.elements.push(elem as T);
		}
	}

	getElements(): DucklingElement[] {
		return this.elements;
	}

	getSemanticTokens() {
		return this.elements.flatMap(elem => elem.getSemanticTokens());
	}
}

export type ListFactory<T extends DucklingElement> = ElementFactory<List<T>, NotStmtFactory>;
export const exprListFactory = new ElementFactory<List<Expr>, NotStmtFactory>(notStmtFactory);

exprListFactory.register("ExprList", List<Expr>);