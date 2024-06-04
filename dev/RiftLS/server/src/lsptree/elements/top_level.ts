import { Decl, declFactory, RiftElement, Stmt, stmtFactory } from "./elements";

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

	getIdentifiers(): RiftElement[] {
		let res: RiftElement[] = [];
		this.statements.forEach((element) => res.concat(element.getIdentifiers()));
		return res;
	}

	getElements(): RiftElement[] {
		return this.statements;
	}

	getSemanticTokens() {
		return this.statements.flatMap(stmt => stmt.getSemanticTokens());
	}
}

declFactory.register("PST", LSPTree);
declFactory.register("LSPTree", LSPTree);