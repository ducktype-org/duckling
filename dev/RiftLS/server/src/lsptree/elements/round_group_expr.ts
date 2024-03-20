import { NotStmt, notStmtFactory } from "./elements";
import { Expr, exprFactory } from "./expression";

export class RoundGroupExpr extends NotStmt {
	expr?: Expr;
	constructor(json: any) {
		super(json);
		if (json["expr"] !== undefined && json["expr"] !== "<nullptr>")
			this.expr = exprFactory.create(json["expr"]);
	}

	getSemanticTokens() {
		return this.expr?.getSemanticTokens() ?? [];
	}
}

notStmtFactory.register("RoundGroupExpr", RoundGroupExpr);