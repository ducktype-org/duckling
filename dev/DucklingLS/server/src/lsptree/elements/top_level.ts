import { Decl, declFactory, DucklingElement, Stmt, stmtFactory } from "./elements";

export class LSPTree extends Decl {
	statements: Stmt[];


	constructor(json: any) {
		super(json);
		this.statements = [];
		for (const stmt_data of json["value"]) {
			const stmt = stmtFactory.create(stmt_data);
			if (stmt) this.statements.push(stmt);
		}
	}

	getIdentifiers(): DucklingElement[] {
		let res: DucklingElement[] = [];
		this.statements.forEach((element) => res.concat(element.getIdentifiers()));
		return res;
	}

	getElements(): DucklingElement[] {
		return this.statements;
	}

	getSemanticTokens() {
		return this.statements.flatMap(stmt => stmt.getSemanticTokens());
	}
}

declFactory.register("PST", LSPTree);
declFactory.register("LSPTree", LSPTree);