import { NotStmt, notStmtFactory, NotStmtFactory } from "./elements";
import { exprFactory, Expr } from "./expression";
import { ElementFactory } from "./element_factory";

export class RetList extends NotStmt {
	rets: Expr[];

	constructor(json: any) {
		super(json);
		this.rets = [];
		for (const ret_params of json["value"]){
			const ret = exprFactory.create(ret_params);
			if (ret) this.rets.push(ret);
		}
	}

	getSemanticTokens() {
		return this.rets.flatMap(ret => ret.getSemanticTokens());
	}
}

export type RetListFactory = ElementFactory<RetList, NotStmtFactory>;
export const retListFactory = new ElementFactory<RetList, NotStmtFactory>(notStmtFactory);

retListFactory.register("RetList", RetList);