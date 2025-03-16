import { CompletionItem, Connection, SemanticTokens, SemanticTokensBuilder, SemanticTokensParams, TextDocumentPositionParams, TextDocuments } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { CompilerDaemonClient } from "./compilerDaemonClient";

export async function handleCompletion(
	_textDocumentPosition: TextDocumentPositionParams,
	documents: TextDocuments<TextDocument>,
	compilerDaemonClient: CompilerDaemonClient,
	connection: Connection
): Promise<CompletionItem[]> {
    const document = documents.get(_textDocumentPosition.textDocument.uri);
    if (!document) return [];

    let compItems = await compilerDaemonClient.getCompletionItems(document.uri,_textDocumentPosition, connection);

    return compItems;
}