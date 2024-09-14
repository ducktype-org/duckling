import { SemanticTokenTypes, SemanticTokenModifiers } from "vscode-languageserver"; 

// Semantic token class used to store the tokens
export interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}

// Function used for sorting the semantic tokens
export function compareTokens(t1: Token, t2: Token): number {
	if (t1.line !== t2.line) {
		return t1.line - t2.line;
	} else if (t1.startCharacter !== t2.startCharacter) {
		return t1.startCharacter - t2.startCharacter;
	}
	return  0;
}

// Semantic tokens legend provides definitions for token types and token modifiers
export const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

// Function used to get the index of a token type
// Sometimes an index is needed instead of the token type itself
export function getTokenTypeIndex(type: any): number {
	return semanticTokensLegend.tokenTypes.indexOf(type);
}

export function stringToSemanticTokenType(value: string): SemanticTokenTypes {
	if (value in SemanticTokenTypes) {
		return SemanticTokenTypes[value as keyof typeof SemanticTokenTypes];
	}
	return SemanticTokenTypes.keyword; // or throw an error, or handle it as needed
}