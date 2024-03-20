import { NotStmt, notStmtFactory, NotStmtFactory } from "./elements";
import { exprFactory, Expr } from "./expression";
import { ElementFactory } from "./element_factory";

export class ParamList extends NotStmt {
	params: Expr[];

	constructor(json: any) {
		super(json);
		this.params = [];
		for (const param_params of json["value"]) {
			const param = exprFactory.create(param_params);
			if (param) this.params.push(param);
		}
	}

	getSemanticTokens() {
		return this.params.flatMap(param => param.getSemanticTokens());
	}
}

type ParamListFactory = ElementFactory<ParamList, NotStmtFactory>;
export const paramListFactory = new ElementFactory<ParamList, NotStmtFactory>(notStmtFactory);

paramListFactory.register("ParamList", ParamList);