import { SemanticTokens, SemanticTokensBuilder, SemanticTokensParams, TextDocuments } from 'vscode-languageserver';
import { SemanticTokenTypes, SemanticTokenModifiers } from 'vscode-languageserver/node';
import {
	TextDocument
} from 'vscode-languageserver-textdocument';

interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}

export const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

function getTokenTypeIndex(type: any): number {
	return semanticTokensLegend.tokenTypes.indexOf(type);
}

// accepts a list of modifiers and return a bit flag representation
function encodeTokenModifiers(modifiers: any): number {
    let bitMask = 0;
    for (const modifier of modifiers) {
        const index = semanticTokensLegend.tokenModifiers.indexOf(modifier);
        if (index !== -1) {
            bitMask |= 1 << index;
        }
    }
    return bitMask;
}

interface CommentMarker {
	line: number;
	startCharacter: number;
	type: 'line' | 'blockStart' | 'blockEnd';
}

function computeCommentsTokens(document: TextDocument): Token[] {
	const tokens: Token[] = [];
	const text = document.getText();
	const lines = text.split(/\r?\n/);
	const markers: CommentMarker[] = [];

	// Collect comment markers
	lines.forEach((line, lineIdx) => {
		let charIdx = 0;
		while (charIdx < line.length) {
			if (line.startsWith("//", charIdx)) {
				markers.push({ line: lineIdx, startCharacter: charIdx, type: 'line' });
				charIdx += 1;
			} else if (line.startsWith("/*", charIdx)) {
				markers.push({ line: lineIdx, startCharacter: charIdx, type: 'blockStart' });
				charIdx += 1;
			} else if (line.startsWith("*/", charIdx)) {
				markers.push({ line: lineIdx, startCharacter: charIdx, type: 'blockEnd' });
				charIdx += 1;
			}
			charIdx += 1;
		}
	});

	let inBlockComment = false;
	let blockStart: CommentMarker | null = null;
	let line_finished = -1;
	for (const marker of markers) {
		if (marker.line === line_finished) continue;

		switch (marker.type) {
			case 'line':
				if (!inBlockComment) {
					tokens.push({
						line: marker.line,
						startCharacter: marker.startCharacter,
						length: lines[marker.line].length - marker.startCharacter,
						tokenType: getTokenTypeIndex(SemanticTokenTypes.comment),
						tokenModifiers: 0 // 0 - no modifiers, same as encodeTokenModifiers()
					});
					line_finished = marker.line;
				}
				break;
			case 'blockStart':
				if (!inBlockComment) {
					inBlockComment = true;
					blockStart = marker;
				}
				break;
			case 'blockEnd':
				if (inBlockComment && blockStart) {
					for (let i = blockStart.line; i <= marker.line; i++) {
						const startChar = i === blockStart.line ? blockStart.startCharacter : 0;
						const endChar = i === marker.line ? marker.startCharacter + 2 : lines[i].length;
						tokens.push({
							line: i,
							startCharacter: startChar,
							length: endChar - startChar,
							tokenType: getTokenTypeIndex(SemanticTokenTypes.comment),
							tokenModifiers: 0
						});
					}
					inBlockComment = false;
					blockStart = null;
				}
				break;
		}
	}
	return tokens;
}

// The function that handles the 'textDocument/semanticTokens/full' request
export async function handleSemanticTokensFull(params: SemanticTokensParams, documents: TextDocuments<TextDocument>): Promise<SemanticTokens> {
	const document = documents.get(params.textDocument.uri);
	if (!document) return { data: [] };

	const tokens = computeCommentsTokens(document);
	const builder = new SemanticTokensBuilder();
	tokens.forEach((token) => {
		builder.push(token.line, token.startCharacter, token.length, token.tokenType, token.tokenModifiers);
	});

	return builder.build();
}
