import { Diagnostic, Connection } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { getDocumentSettings } from "./server";
import { RiftParserError } from "./errors";
import { CompilerDaemonClient } from "./compilerDaemonClient";

export async function validateRift(
	textDocument: TextDocument, 
	connection: Connection, 
	compilerDaemonClient: CompilerDaemonClient
): Promise<void> {
	const settings = await getDocumentSettings(textDocument.uri);
	let problems = 0;
	const diagnostics: Diagnostic[] = [];
	let errors = await compilerDaemonClient.getErrors(textDocument.uri, connection);

	for (const error of errors) {
		problems++;
		const diagnostic: Diagnostic = {
			severity: error.severity,
			range: {
				start: { line: error.line - 1, character: error.column - 1 },
				end: { line: error.line - 1, character: error.column }
			},
			message: error.message,
			source: "Rift " + error.type
		};
		diagnostics.push(diagnostic);

		if (problems > settings.maxNumberOfProblems) {
			break;
		}
	}

	// Send the computed diagnostics to the client
	connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}
