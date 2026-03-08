import { Connection, Position, TextDocumentPositionParams, TextDocuments } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { Location } from "vscode-languageserver/node";



function positionToOffset(position: Position, text: string): number {
    const lines = text.split(/\r?\n/); // Split the text into lines
    let offset = 0;

    for (let i = 0; i < position.line; i++) {
        offset += lines[i].length + 1; // Add the length of the line + 1 for the newline character
    }

    offset += position.character; // Add the character offset within the line
    return offset;
}

export async function handleDefinition(
    params: TextDocumentPositionParams,
    documents: TextDocuments<TextDocument>,
    compilerDaemonClient: CompilerDaemonClient,
    connection: Connection
) : Promise<Location | Location[] | null> {
    const document = documents.get(params.textDocument.uri);
    if (!document) return null;

    const text = document.getText();
    const offset = positionToOffset(params.position, text);

    let definition = await compilerDaemonClient.getDefinition(params, offset, connection);
    if (!definition) return null;

    return definition;
}