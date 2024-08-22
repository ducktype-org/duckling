import { SemanticTokenTypes, SemanticTokenModifiers, SemanticTokens } from "vscode-languageserver";
import { Token, getTokenTypeIndex } from "../../semanticTokensDeclarations";

export class SourcePosition {
	startLine: number;
	startColumn: number;
	endLine: number;
	endColumn: number;
	start: number;
	end: number;
	constructor(position: { [key: string]: number }) {
		this.startLine = position["startLine"];
		this.startColumn = position["startColumn"];
		this.endLine = position["endLine"];
		this.endColumn = position["endColumn"];
		this.start = position["start"];
		this.end = position["end"];
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

	static stringToSemanticTokenType(value: string): SemanticTokenTypes {
		if (value in SemanticTokenTypes) {
			return SemanticTokenTypes[value as keyof typeof SemanticTokenTypes];
		}
		return SemanticTokenTypes.keyword; // or throw an error, or handle it as needed
	}

	static parseTree(json: any): SemanticToken[] {
		const result: SemanticToken[] = [];

		type Node = {
			[key: string]: any;
		};
	
		function traverse(node: Node): void {
			const keys = Object.keys(node);
			if (keys.length === 0) return;
	
			const firstField = keys[0]; // Assume first field is the first key
			const position = new SourcePosition(node.position);
			const semTokenType = node.semTokenType;
	
			// Push the current node's tuple to the result array
			if (typeof firstField === 'string' && typeof semTokenType === 'string') {
				result.push(SemanticToken.fromPosition(position, SemanticToken.stringToSemanticTokenType(semTokenType), []));
			}
	
			// Recursively traverse the rest of the fields as children nodes
			keys.forEach(key => {
				if (key !== firstField && key !== 'position' && key !== 'semTokenType') {
					const childNode = node[key];
					if (typeof childNode === 'object' && childNode !== null) {
						traverse(childNode);
					}
				}
			});
		}
	
		traverse(json);
		return result;
	}
}