import { Diagnostic, Connection, DocumentUri } from "vscode-languageserver";
import { getDocumentSettings } from "./server";
import { CompilerDaemonClient } from "./compilerDaemonClient";

export async function validateDuckling(
	uri: DocumentUri, 
	connection: Connection, 
	compilerDaemonClient: CompilerDaemonClient
): Promise<void> {
	const settings = await getDocumentSettings(uri);

	// Get the errors from the compiler daemon
	let errorsMap = await compilerDaemonClient.getErrors(uri, connection);

	for (const [uri, diagnostics] of Object.entries(errorsMap)) {
		const filteredDiagnostics = diagnostics.slice(0, settings.maxNumberOfProblems);

		// Send the computed diagnostics to the client
		connection.sendDiagnostics({ uri: uri, diagnostics: filteredDiagnostics });
	}
}
