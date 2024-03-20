import { SemanticToken } from "./common";
import { Stmt, stmtFactory } from "./elements";
import { type Expr, exprFactory } from "./expression";
import { SemanticTokenTypes } from "vscode-languageserver";

abstract class Action extends Stmt {
	expr?: Expr;

	constructor (json: any, keyword: string) {
		super(json);
		if (json[keyword] !== undefined)
			this.expr = exprFactory.create(json[keyword]);
	}

	getSemanticTokens (): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		return tokens.concat(this.expr?.getSemanticTokens() ?? []);
	}
}

class Return extends Action {
	constructor (json: any) {
		super(json, "with");
	}
}

stmtFactory.register("Return", Return);

class Break extends Action {
	constructor (json: any) {
		super(json, "from");
	}
}

stmtFactory.register("Break", Break);

class Continue extends Action {
	constructor (json: any) {
		super(json, "with");
	}
}

stmtFactory.register("Continue", Continue);

class Redo extends Action {
	constructor (json: any) {
		super(json, "what");
	}
}

stmtFactory.register("Redo", Redo);

class Restart extends Action {
	constructor (json: any) {
		super(json, "what");
	}
}

stmtFactory.register("Restart", Restart);

class Defer extends Action {
	constructor (json: any) {
		super(json, "statements");
	}
}

stmtFactory.register("Defer", Defer);

class Throw extends Action {
	constructor (json: any) {
		super(json, "exception");
	}
}

stmtFactory.register("Throw", Throw);
