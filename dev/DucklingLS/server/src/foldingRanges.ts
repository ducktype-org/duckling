import { TextDocument } from 'vscode-languageserver-textdocument';
import { FoldingRange, FoldingRangeKind, TextDocuments, FoldingRangeParams } from 'vscode-languageserver';
import { Token, getTokenTypeIndex } from "./semanticTokensDeclarations";
import { DucklingElement } from './lsptree/elements/elements';

/**
 * Represents a mapping of token types to their corresponding families.
 * If we set tokens to one family, they will fold together.
 */
const tokenFamilies: { [key: number]: number } = {
	[getTokenTypeIndex('keyword')]: 1,
	[getTokenTypeIndex('variable')]: 1,
	[getTokenTypeIndex('operator')]: 1,
	[getTokenTypeIndex('number')]: 1,
	[getTokenTypeIndex('comment')]: 2,
	[getTokenTypeIndex('string')]: 3,
	[getTokenTypeIndex('decorator')]: 4,
};

/**
 * Retrieves the folding ranges for a given text document.
 * @param params - The parameters for the folding range request.
 * @param documents - The collection of text documents.
 * @param semanticTokensCache - Cache of semantic tokens.
 * @param pstCache - Cache of Duckling elements.
 * @returns An array of FoldingRanges.
 */
export function getFoldingRanges(
	params: FoldingRangeParams,
	documents: TextDocuments<TextDocument>,
	semanticTokensCache: Map<string, Token[]>,
	pstCache: Map<string, DucklingElement | null>
): FoldingRange[] {
	const document = documents.get(params.textDocument.uri) as TextDocument;
	if (!document) return []; 

	const LSPTree = pstCache.get(document.uri);
	const foldingRangesPST: any =  LSPTree?.getFoldingRanges() ?? [];
	const foldingRanges: FoldingRange[] = [];
	const tokens = semanticTokensCache.get(document.uri) || [];
	foldingRanges.push(...foldSemTokens(tokens, document));
	foldingRanges.push(...foldingRangesPST);

	return foldingRanges;
}

/**
 * Generates folding ranges based on the provided tokens and document.
 * @param tokens - The array of tokens to generate folding ranges for.
 * @param document - The text document.
 * @returns An array of FoldingRanges.
 */
function foldSemTokens(tokens: Token[], document: TextDocument): FoldingRange[] {
	const foldingRanges: FoldingRange[] = [];
	let currentFamily: number | null = null; // Current token family being processed
	let startToken: Token | null = null; // Start token of the current folding range
	let lastTokenLine: number = 0; // Line number of the last token processed

	tokens.forEach((token, i) => {
		const tokenFamily = tokenFamilies[token.tokenType]; // Get the family of the current token
		// Check if the current token belongs to the same family or is a comment and is on a consecutive line
		if ((currentFamily === tokenFamily || 
			token.tokenType === getTokenTypeIndex('comment')) 
			&& token.line - lastTokenLine <= 1) {
			lastTokenLine = token.line; // Update the last token line
			return; // Continue to the next token
		}
		// If the current token does not belong to the same family and is not on a consecutive line
		if (currentFamily !== null && startToken !== null && token.line > startToken.line + 1) {
			// Create a folding range from the start token to the previous token
			foldingRanges.push(createFoldingRange(startToken, tokens[i - 1], document));
		}
		// Update the start token and current family if the token family is defined
		if (tokenFamily !== undefined) {
			startToken = token;
			currentFamily = tokenFamily;
		} else {
			currentFamily = null;
			startToken = null;  
		}
		lastTokenLine = token.line; // Update the last token line
	});
	// Create a folding range for the last set of tokens if applicable
	if (currentFamily !== null && startToken !== null && tokens[tokens.length - 1].line > startToken['line'] + 1) {
		foldingRanges.push(createFoldingRange(startToken, tokens[tokens.length - 1], document));
	}
	return foldingRanges; // Return the generated folding ranges
}

/**
 * Creates a folding range based on the provided start and end tokens.
 * @param startToken - The start token of the folding range.
 * @param endToken - The end token of the folding range.
 * @param document - The text document.
 * @returns A FoldingRange object.
 */
function createFoldingRange(startToken: Token, endToken: Token, document: TextDocument): FoldingRange {
	return {
		startLine: startToken.line,
		startCharacter: startToken.startCharacter,
		endLine: endToken.line,
		endCharacter: endToken.startCharacter + endToken.length,
		kind: FoldingRangeKind.Region
	};
}
