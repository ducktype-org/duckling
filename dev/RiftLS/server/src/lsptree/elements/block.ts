import { CodeDecl, codeDeclFactory, RiftElement } from "./elements";
import { Identifier, OptionalIdentifier } from "./identifier";
import { CodeBlock, codeBlockFactory } from "./code_block";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";

export class Block extends CodeDecl {
	name?: Identifier;
	code_block: CodeBlock;

	constructor(json: { [key: string]: any }) {
		super(json);
		this.name = OptionalIdentifier.create(json["name"]);
		this.code_block = codeBlockFactory.createDefined(json["code_block"]);
	}

	getElements(): RiftElement[] {
		const elements: RiftElement[] = [this.code_block];
		if (this.name)
			elements.push(this.name);
		return elements;
	}

	getSemanticTokens() {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		if (this.name)
			tokens.push(...this.name.getSemanticTokens());
		tokens.push(...this.code_block.getSemanticTokens());
		return tokens;
	}
}

codeDeclFactory.register("Block", Block);