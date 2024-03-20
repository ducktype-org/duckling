import { SemanticTokenTypes, SemanticTokenModifiers, SemanticTokens } from "vscode-languageserver";
import { Token, getTokenTypeIndex } from "../../semanticTokensDeclarations";

export class SourcePosition {
	line: number;
	column: number;
	begin: number;
	end: number;
	path: string;
	constructor(position: {[key: string]: number}, path: string = "") {
		this.line = position["line"];
		this.column = position["column"];
		this.begin = position["start"];
		this.end = position["end"];

		this.path = path;
	}

}

export class SemanticToken implements Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
	constructor(line: number, char: number, length: number, tokenType: SemanticTokenTypes, tokenModifiers: SemanticTokenModifiers[]) {
		this.line = line-1;
		this.startCharacter = char-1;
		this.length = length;
		this.tokenType = getTokenTypeIndex(tokenType);
		this.tokenModifiers = 0;
	}

	static fromPosition(position: SourcePosition, tokenType: SemanticTokenTypes, tokenModifiers: SemanticTokenModifiers[]) {
		return new SemanticToken(position.line, position.column, position.end - position.begin + 1, tokenType , tokenModifiers);
	}
}