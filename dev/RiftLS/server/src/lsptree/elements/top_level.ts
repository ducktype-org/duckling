import { Decl, declFactory, Stmt, stmtFactory } from "./elements";

export class LSPTree extends Decl{
	statements: Stmt[];

    
	constructor(json: any) {
		super(json);
		this.statements = [];
		for (const stmt_data of json["value"]) {
			const stmt = stmtFactory.create(stmt_data);
			if (stmt) this.statements.push(stmt);
		}
	}

	getSemanticTokens() {
		return this.statements.flatMap(stmt => stmt.getSemanticTokens());
	}
}

declFactory.register("PST", LSPTree);
declFactory.register("LSPTree", LSPTree);