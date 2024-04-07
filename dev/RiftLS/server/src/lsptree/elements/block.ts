import { CodeDecl, codeDeclFactory } from "./elements";
import { Identifier, OptionalIdentifier } from "./identifier";
import { CodeBlock, codeBlockFactory } from "./code_block";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";

export class Block extends CodeDecl {
	name?: Identifier;
	code_block: CodeBlock;

	constructor(json: {[key: string]: any}) {
		super(json);
		this.name = OptionalIdentifier.create(json["optional name"]);
		this.code_block = codeBlockFactory.createDefined(json["code block"]);
	}

	getElements() {
		const elements: CodeDecl[] = [];
		if (this.name)
			elements.push(this.name);
		elements.push(this.code_block);
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