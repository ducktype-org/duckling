import { Connection, TextDocumentPositionParams, TextDocuments } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { Location } from "vscode-languageserver/node";


export async function handleDefinition(
    params: TextDocumentPositionParams,
    documents: TextDocuments<TextDocument>,
    compilerDaemonClient: CompilerDaemonClient,
    connection: Connection
) : Promise<Location | Location[] | null> {
    const document = documents.get(params.textDocument.uri);
    if (!document) return null;

    let definition = await compilerDaemonClient.getDefinition(params, connection);


    return definition;
}