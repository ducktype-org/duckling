import { Connection, SemanticTokens, SemanticTokensBuilder, SemanticTokensParams, TextDocuments } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { CompilerDaemonClient } from "./compilerDaemonClient";

interface CommentMarker {
	line: number;
	startCharacter: number;
	type: "line" | "blockStart" | "blockEnd";
}


interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}

// Function used for sorting the semantic tokens
function compareTokens(t1: Token, t2: Token): number {
	if (t1.line !== t2.line) {
		return t1.line - t2.line;
	} else if (t1.startCharacter !== t2.startCharacter) {
		return t1.startCharacter - t2.startCharacter;
	}
	return  0;
}


// The function that handles the 'textDocument/semanticTokens/full' request
export async function handleSemanticTokensFull(
	params: SemanticTokensParams,
	documents: TextDocuments<TextDocument>,
	compilerDaemonClient: CompilerDaemonClient,
	connection: Connection
): Promise<SemanticTokens> {
	const document = documents.get(params.textDocument.uri);
	if (!document) return { data: [] };

	let semTokens = await compilerDaemonClient.getSemanticTokens(document.uri, connection);

	// Sort the tokens and build the response
	semTokens.sort((a, b) => compareTokens(a, b));
	const builder = new SemanticTokensBuilder();
	// Each token needs to be pushed to the builder
	semTokens.forEach((token) => {
		builder.push(token.line, token.startCharacter, token.length, token.tokenType, token.tokenModifiers);
	});

	// The builder will translate the tokens to the LSP format by itself
	return builder.build();
}