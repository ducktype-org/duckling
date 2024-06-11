import { TextDocument } from 'vscode-languageserver-textdocument';
import { FoldingRange, FoldingRangeKind, TextDocuments, FoldingRangeParams } from 'vscode-languageserver';
import { Token, getTokenTypeIndex } from "./semanticTokensDeclarations";
import { DucklingElement } from './lsptree/elements/elements';

// If we set tokens to one family, they will fold
const tokenFamilies: { [key: number]: number } = {
	[getTokenTypeIndex('keyword')]: 1,
	[getTokenTypeIndex('variable')]: 1,
	[getTokenTypeIndex('operator')]: 1,
	[getTokenTypeIndex('comment')]: 2,
	[getTokenTypeIndex('string')]: 3,
	[getTokenTypeIndex('decorator')]: 4,
};

export function getFoldingRanges(
	params: FoldingRangeParams,
	documents: TextDocuments<TextDocument>,
	semanticTokensCache: Map<string, Token[]>,
	pstCache: Map<string, DucklingElement | null>
): FoldingRange[] {
	const document = documents.get(params.textDocument.uri) as TextDocument;
	if (!document)  return []; 

	const LSPTree = pstCache.get(document.uri);
	const foldingRangesPST: any =  LSPTree?.getFoldingRanges() ?? [];
	const foldingRanges: FoldingRange[] = [];
	const tokens = semanticTokensCache.get(document.uri) || [];
	foldingRanges.push(...foldSemTokens(tokens, document));
	foldingRanges.push(...foldingRangesPST);

	return foldingRanges;
}

function foldSemTokens(tokens: Token[], document: TextDocument): FoldingRange[] {
	const foldingRanges: FoldingRange[] = [];
	let currentFamily: number | null = null;
	let startToken: Token | null = null;
	let lastTokenLine: number = 0;

	tokens.forEach((token, i) => {
		const tokenFamily = tokenFamilies[token.tokenType];
		if ((currentFamily === tokenFamily || 
			token.tokenType === getTokenTypeIndex('comment')) 
			&& token.line - lastTokenLine <= 1) {
			lastTokenLine = token.line;
			return;
		}
		if (currentFamily !== null && startToken !== null && token.line > startToken.line + 1) {
			foldingRanges.push(createFoldingRange(startToken, tokens[i - 1], document));
		}
		if (tokenFamily !== undefined) {
			startToken = token;
			currentFamily = tokenFamily;
		} else {
			currentFamily = null;
			startToken = null;  
		}
		lastTokenLine = token.line;
	});
	if (currentFamily !== null && startToken !== null && tokens[tokens.length - 1].line > startToken['line'] + 1) {
		foldingRanges.push(createFoldingRange(startToken, tokens[tokens.length - 1], document));
	}
	return foldingRanges;
}


function createFoldingRange(startToken: Token, endToken: Token, document: TextDocument): FoldingRange {
	return {
		startLine: startToken.line,
		startCharacter: startToken.startCharacter,
		endLine: endToken.line,
		endCharacter: endToken.startCharacter + endToken.length,
		kind: FoldingRangeKind.Region
	};
}
