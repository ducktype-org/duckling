import { CodeBlock, codeBlockFactory } from "./code_block";
import { Identifier, identifierFactory } from "./identifier";
import { SemanticTokenTypes } from "vscode-languageserver";
import { Decl, declFactory, RiftElement } from "./elements";
import { SemanticToken } from "./common";

export class Namespace extends Decl {
	name?: Identifier;
	body?: CodeBlock;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.create(json["name"]);
		this.body = codeBlockFactory.create(json["block"]);
	}

	getElements(): RiftElement[] {
		const elements: RiftElement[] = [];
		if (this.name)
			elements.push(this.name);
		if (this.body)
			elements.push(this.body);
		return elements;
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		if (this.name)
			tokens.push(...this.name.getSemanticTokens());
		if (this.body)
			tokens.push(...this.body.getSemanticTokens());
		return tokens;
	}
}

declFactory.register("Namespace", Namespace);