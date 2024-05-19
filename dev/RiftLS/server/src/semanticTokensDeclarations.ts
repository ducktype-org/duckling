import { SemanticTokenTypes, SemanticTokenModifiers } from "vscode-languageserver"; 

export interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}

export function compareTokens(t1: Token, t2: Token): number {
	if (t1.line !== t2.line) {
		return t1.line - t2.line;
	} else if (t1.startCharacter !== t2.startCharacter) {
		return t1.startCharacter - t2.startCharacter;
	}
	return  0;
}

export const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

export function getTokenTypeIndex(type: any): number {
	return semanticTokensLegend.tokenTypes.indexOf(type);
}