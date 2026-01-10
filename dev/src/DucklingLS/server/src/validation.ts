import { Diagnostic, Connection } from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { getDocumentSettings } from "./server";
import { CompilerDaemonClient } from "./compilerDaemonClient";

export async function validateDuckling(
	textDocument: TextDocument, 
	connection: Connection, 
	compilerDaemonClient: CompilerDaemonClient
): Promise<void> {
	const settings = await getDocumentSettings(textDocument.uri);

	// Get the errors from the compiler daemon
	let errorsMap = await compilerDaemonClient.getErrors(textDocument.uri, connection);

	for (const [uri, diagnostics] of Object.entries(errorsMap)) {
		let problems = 0;
		const filteredDiagnostics: Diagnostic[] = [];

		for (const diagnostic of diagnostics) {
			problems++;
			filteredDiagnostics.push(diagnostic);
			if (problems > settings.maxNumberOfProblems) {
				break;
			}
		}

		// Send the computed diagnostics to the client
		connection.sendDiagnostics({ uri: uri, diagnostics: filteredDiagnostics });
	}
}
