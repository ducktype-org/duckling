import {
	Diagnostic,
	DiagnosticSeverity,
	Connection
} from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";
import { getDocumentSettings } from "./server";

export async function validateTextDocument(textDocument: TextDocument, connection: Connection): Promise<void> {
	// Example validation logic
	const settings = await getDocumentSettings(textDocument.uri);
	const text = textDocument.getText();
	const pattern = /\b[A-Z]{2,}\b/g;
	let m: RegExpExecArray | null;

	let problems = 0;
	const diagnostics: Diagnostic[] = [];
	while ((m = pattern.exec(text)) && problems < settings.maxNumberOfProblems) {
		problems++;
		const diagnostic: Diagnostic = {
			severity: DiagnosticSeverity.Warning,
			range: {
				start: textDocument.positionAt(m.index),
				end: textDocument.positionAt(m.index + m[0].length)
			},
			message: `${m[0]} is all uppercase.`,
			source: "ex"
		};
		diagnostics.push(diagnostic);
	}

	// Send the computed diagnostics to the client
	connection.sendDiagnostics({ uri: textDocument.uri, diagnostics });
}