import { Connection, SemanticTokens, SemanticTokensBuilder, SemanticTokensParams, TextDocuments } from "vscode-languageserver";
import { SemanticTokenTypes } from "vscode-languageserver/node";
import { TextDocument } from "vscode-languageserver-textdocument";
import { Token, getTokenTypeIndex, semanticTokensLegend, compareTokens } from "./semanticTokensDeclarations";
import { DucklingElement } from "./lsptree/elements/elements";
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { extractObjectsWithTokenKind } from "./parseLSPT";
import { log } from "console";

interface CommentMarker {
	line: number;
	startCharacter: number;
	type: "line" | "blockStart" | "blockEnd";
}

// This function computes the tokens for comments separately
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
				markers.push({ line: lineIdx, startCharacter: charIdx, type: "line" });
				charIdx += 1;
			} else if (line.startsWith("/*", charIdx)) {
				markers.push({ line: lineIdx, startCharacter: charIdx, type: "blockStart" });
				charIdx += 1;
			} else if (line.startsWith("*/", charIdx)) {
				markers.push({ line: lineIdx, startCharacter: charIdx, type: "blockEnd" });
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
			case "line":
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
			case "blockStart":
				if (!inBlockComment) {
					inBlockComment = true;
					blockStart = marker;
				}
				break;
			case "blockEnd":
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
export async function handleSemanticTokensFull(
	params: SemanticTokensParams,
	documents: TextDocuments<TextDocument>,
	semanticTokensCache: Map<string, Token[]>,
	compilerDaemonClient: CompilerDaemonClient,
	connection: Connection
): Promise<SemanticTokens> {
	const document = documents.get(params.textDocument.uri);
	if (!document) return { data: [] };

	// Get the LSPTree from the cache
	let preloadedTokens = semanticTokensCache.get(document.uri);
	let tokens: Token[] = [];
	// or request it from the compiler daemon if it is not in the cache
	if (preloadedTokens === undefined) {
		let LSPTree = await compilerDaemonClient.getLSPT(document.uri, connection);
		if (!LSPTree) {
			connection.console.error("LSPTree is null");
			return { data: [] };
		}
		preloadedTokens = extractObjectsWithTokenKind(LSPTree);
		const commentTokens: Token[] = computeCommentsTokens(document);
		tokens = preloadedTokens.concat(commentTokens);
		tokens.sort((a, b) => compareTokens(a, b));
		semanticTokensCache.set(document.uri, tokens);
	} else {
		tokens = preloadedTokens;
		connection.console.log("Using preloaded tokens");
	}

	const builder = new SemanticTokensBuilder();
	// Each token needs to be pushed to the builder
	tokens.forEach((token) => {
		builder.push(token.line, token.startCharacter, token.length, token.tokenType, token.tokenModifiers);
	});

	// The builder will translate the tokens to the LSP format by itself
	return builder.build();
}