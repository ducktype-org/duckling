import { Connection } from 'vscode-languageserver';
import {TextDocument } from 'vscode-languageserver-textdocument';
import { FoldingRange, FoldingRangeKind, TextDocuments, FoldingRangeParams } from 'vscode-languageserver';
import { Token, getTokenTypeIndex } from "./semanticTokensDeclarations";
import { DucklingElement } from './lsptree/elements/elements';
import { extractObjectsWithFoldingRange } from './parseLSPT';
import { CompilerDaemonClient } from "./compilerDaemonClient";

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
 * @param foldingCache - Cache of folding ranges.
 * @returns An array of FoldingRanges.
 */
export async function getFoldingRanges(
	params: FoldingRangeParams,
	documents: TextDocuments<TextDocument>,
	compilerDaemonClient: CompilerDaemonClient,
	foldingCache: Map<string, FoldingRange[]>,
	connection: Connection
): Promise<FoldingRange[]> {
	const document = documents.get(params.textDocument.uri) as TextDocument;
	if (!document) return []; 

	let tokens = foldingCache.get(document.uri) || null;
	if (!tokens) {
		// We now know we need to generate the ranges from scratch.
		let LSPTree = await compilerDaemonClient.getLSPT(document.uri, connection);
		const newTokens = groupFoldingRanges(extractObjectsWithFoldingRange(LSPTree));
		foldingCache.set(document.uri, newTokens);
		tokens = newTokens;
	}
	
	return tokens;
}

function groupFoldingRanges(tokens: FoldingRange[]): FoldingRange[] {
    if (tokens.length === 0) return [];

    const groupedRanges: FoldingRange[] = [];
    let startToken = tokens[0];

    for (let i = 1; i < tokens.length; i++) {
        const token = tokens[i];

        // If startToken is null, initialize it with the current token
        if (!startToken) {
            startToken = token;
            continue;
        }

        // Check if the current token can be grouped with the startToken
        if (token.kind === startToken.kind && token.kind !== "region" && token.startLine === startToken.endLine + 1) {
            // Extend the endLine of the startToken to the endLine of the current token
            startToken.endLine = token.endLine;
			startToken.endCharacter = token.endCharacter;
        } else {
            // Push the current group to the groupedRanges array
            groupedRanges.push(createFoldingRange(startToken, startToken));
            // Start a new group with the current token
            startToken = token;
        }
    }

    // Push the last group to the groupedRanges array
    if (startToken) {
        groupedRanges.push(createFoldingRange(startToken, startToken));
    }

    return groupedRanges;
}









/**
 * Generates folding ranges based on the provided tokens and document.
 * @param tokens - The array of tokens to generate folding ranges for.
 * @param document - The text document.
 * @returns An array of FoldingRanges.
 */
// function foldSemTokens(tokens: FoldingRange[], document: TextDocument): FoldingRange[] {
// 	if (tokens.length === 0) return [];
// 	const foldingRanges: FoldingRange[] = [];
// 	let currentFamily: string | null = null; // Current token family being processed
// 	let startToken: FoldingRange | null = null; // Start token of the current folding range
// 	let lastTokenLine: number = 0; // Line number of the last token processed

// 	tokens.forEach((token, i) => {
// 		const tokenFamily = token.kind; // Get the family of the current token
// 		// Check if the current token belongs to the same family or is a comment and is on a consecutive line
// 		if ((currentFamily === tokenFamily || 
// 			token.kind === 'comment') 
// 			&& token.startLine - lastTokenLine <= 1) {
// 			lastTokenLine = token.endLine; // Update the last token line
// 			return; // Continue to the next token
// 		}
// 		// If the current token does not belong to the same family and is not on a consecutive line
// 		if (currentFamily !== null && startToken !== null && token.startLine > startToken.endLine + 1) {
// 			// Create a folding range from the start token to the previous token
// 			foldingRanges.push(createFoldingRange(startToken, tokens[i - 1]));
// 		}
// 		// Update the start token and current family if the token family is defined
// 		if (tokenFamily !== undefined) {
// 			startToken = token;
// 			currentFamily = tokenFamily;
// 		} else {
// 			currentFamily = null;
// 			startToken = null;  
// 		}
// 		lastTokenLine = token.endLine; // Update the last token line
// 	});
// 	// Create a folding range for the last set of tokens if applicable
// 	if (currentFamily !== null && startToken !== null && tokens[tokens.length - 1].startLine > startToken.endLine + 1) {
// 		foldingRanges.push(createFoldingRange(startToken, tokens[tokens.length - 1]));
// 	}
// 	return foldingRanges; // Return the generated folding ranges
// }

/**
 * Creates a folding range based on the provided start and end tokens.
 * @param startToken - The start token of the folding range.
 * @param endToken - The end token of the folding range.
 * @param document - The text document.
 * @returns A FoldingRange object.
 */
function createFoldingRange(startToken: FoldingRange, endToken: FoldingRange): FoldingRange {
	return {
		startLine: startToken.startLine,
		startCharacter: startToken.startCharacter,
		endLine: endToken.endLine,
		endCharacter: endToken.endCharacter,
		kind: startToken.kind,
	};
}
