import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Expr } from "./expression";
import { Identifier, identifierFactory } from "./identifier";
import { exprListFactory, List } from "./list";

export class Attribute extends Stmt {
	name: Identifier;
	args?: List<Expr>;
	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		if (json["args"])
			this.args = exprListFactory.createDefined(json["args"]);
	}

	getElements(): RiftElement[] {
		const elements: RiftElement[] = [this.name];
		if (this.args)
			elements.push(this.args);
		return elements;
	}

	getSemanticTokens() {
		const tokens = this.name.getSemanticTokens();
		if (this.args)
			tokens.push(...this.args.getSemanticTokens());
		return tokens;
	}
}

stmtFactory.register("Attribute", Attribute);