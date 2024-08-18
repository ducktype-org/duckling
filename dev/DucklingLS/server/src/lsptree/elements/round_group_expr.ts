import { NotStmt, notStmtFactory, DucklingElement } from "./elements";
import { Expr, exprFactory } from "./expression";

export class RoundGroupExpr extends NotStmt {
	expr?: Expr;
	constructor(json: any) {
		super(json);
		if (json["expr"] !== undefined && json["expr"] !== "<nullptr>")
			this.expr = exprFactory.create(json["expr"]);
	}

	getElements(): DucklingElement[] {
		return this.expr?.getElements() ?? [];
	}

	getSemanticTokens() {
		return this.expr?.getSemanticTokens() ?? [];
	}
}

notStmtFactory.register("RoundGroupExpr", RoundGroupExpr);