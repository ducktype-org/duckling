import { SemanticTokenTypes, SemanticTokenModifiers, SemanticTokens } from "vscode-languageserver";
import { Token, getTokenTypeIndex } from "../../semanticTokensDeclarations";

export class SourcePosition {
	startLine: number;
	startColumn: number;
	endLine: number;
	endColumn: number;
	start: number;
	end: number;
	path: string;
	constructor(position: { [key: string]: number }, path: string = "") {
		this.startLine = position["startLine"];
		this.startColumn = position["startColumn"];
		this.endLine = position["endLine"];
		this.endColumn = position["endColumn"];
		this.start = position["start"];
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
		this.line = line - 1;
		this.startCharacter = char - 1;
		this.length = length;
		this.tokenType = getTokenTypeIndex(tokenType);
		this.tokenModifiers = 0;
	}

	static fromPosition(position: SourcePosition, tokenType: SemanticTokenTypes, tokenModifiers: SemanticTokenModifiers[]) {
		return new SemanticToken(position.startLine, position.startColumn, position.end - position.start + 1, tokenType, tokenModifiers);
	}
}