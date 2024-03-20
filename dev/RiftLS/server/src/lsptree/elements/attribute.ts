import { Stmt, stmtFactory } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { ArgList } from "./argument_list";

export class Attribute extends Stmt {
	name: Identifier;
	args?: ArgList; // TODO in compiler
	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
	}

	getSemanticTokens() {
		const tokens = this.name.getSemanticTokens();
		if (this.args)
			tokens.push(...this.args.getSemanticTokens());
		return tokens;
	}
}

stmtFactory.register("Attribute", Attribute);