import { SemanticTokenTypes } from "vscode-languageserver";
import { keyword } from ".";
import { CodeBlock, codeBlockFactory } from "./code_block";
import { SemanticToken } from "./common";
import { Decl, declFactory, DucklingElement } from "./elements";
import { Expr, exprFactory } from "./expression";
import { Identifier, identifierFactory } from "./identifier";


export class Struct extends Decl {
	name: Identifier;
	bases: Expr[] = [];
	body?: CodeBlock;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		for (const base_class_params of json["bases"]) {
			const base_class = exprFactory.create(base_class_params);
			if (base_class) this.bases.push(base_class);
		}
		this.body = codeBlockFactory.create(json["body"]);
	}

	getElements(): DucklingElement[] {
		const elements: DucklingElement[] = [this.name];
		this.bases.forEach(base =>
			elements.push(base)
		);
		if (this.body)
			elements.push(this.body);
		return elements;
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		this.bases.forEach(base =>
			tokens.push(...base.getSemanticTokens())
		);
		if (this.body)
			tokens.push(...this.body.getSemanticTokens());
		return tokens;
	}
}

declFactory.register("Struct", Struct);