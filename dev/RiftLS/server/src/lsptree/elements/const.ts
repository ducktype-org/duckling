import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Identifier } from "./identifier";
import { unpackJSONObject } from "./utils";
import { Expr, exprFactory } from "./expression";
import { identifierFactory } from "./identifier";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";

export class Const extends Stmt {
	name: Identifier;
	type?: Expr;
	value?: Expr;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json.get("name"));
		this.type = exprFactory.create(json.get("type"));
		this.value = exprFactory.create(json.get("value"));
	}
	
	getElements(): RiftElement[] {
		const elements: RiftElement[] = [this.name];
		if (this.type) elements.push(this.type);
		if (this.value) elements.push(this.value);
		return elements;
	}

	getSemanticTokens() {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		tokens.push(...this.name.getSemanticTokens());
		if (this.type) tokens.push(...this.type.getSemanticTokens());
		if (this.value) tokens.push(...this.value.getSemanticTokens());
		return tokens;
	}
}

stmtFactory.register("Const", Const);

