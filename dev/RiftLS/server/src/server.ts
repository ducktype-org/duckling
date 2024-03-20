/* --------------------------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License. See License.txt in the project root for license information.
 * ------------------------------------------------------------------------------------------ */
import {
	createConnection,
	TextDocuments,
	ProposedFeatures,
	InitializeParams,
	DidChangeConfigurationNotification,
	TextDocumentSyncKind,
	InitializeResult,
	SemanticTokenTypes,
	SemanticTokenModifiers
} from "vscode-languageserver/node";

import {
	TextDocument
} from "vscode-languageserver-textdocument";
import { handleSemanticTokensFull } from "./semanticTokens";
import { onCompletion, onCompletionResolve } from "./completion";
import { validateTextDocument } from "./validation"; // Import the validation function
import { getPST } from "./compilerInterface"; // Import the compiler interface function
require("./lsptree/elements/index");


// Create a connection for the server, using Node's IPC as a transport.
// Also include all preview / proposed LSP features.
const connection = createConnection(ProposedFeatures.all);

// Create a simple text document manager.
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

// Semantic tokens legend, only 'comment' token type for now
const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

connection.onInitialize((params: InitializeParams) => {
	const capabilities = params.capabilities;

	// Does the client support the `workspace/configuration` request?
	// If not, we fall back using global settings.
	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);
	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);
	hasDiagnosticRelatedInformationCapability = !!(
		capabilities.textDocument &&
		capabilities.textDocument.publishDiagnostics &&
		capabilities.textDocument.publishDiagnostics.relatedInformation
	);

	const result: InitializeResult = {
		capabilities: {
			textDocumentSync: TextDocumentSyncKind.Incremental,
			// Tell the client that this server supports those options
			// Tell the client that this server supports those options
			completionProvider: {
				resolveProvider: true
			},
			semanticTokensProvider: {
				legend: semanticTokensLegend,
				full: true,
			},
		}
	};
	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true
			}
		};
	}
	return result;
});

connection.onInitialized(() => {
	if (hasConfigurationCapability) {
		// Register for all configuration changes.
		connection.client.register(DidChangeConfigurationNotification.type, undefined);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders(_event => {
			connection.console.log("Workspace folder change event received.");
		});
	}
});

connection.onRequest("textDocument/semanticTokens/full", (params) => handleSemanticTokensFull(params, documents));

// The example settings
interface ExampleSettings {
	maxNumberOfProblems: number;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
// Please note that this is not the case when using this server with the client provided in this example
// but could happen with other clients.
const defaultSettings: ExampleSettings = { maxNumberOfProblems: 1000 };
let globalSettings: ExampleSettings = defaultSettings;

// Cache the settings of all open documents
const documentSettings: Map<string, Thenable<ExampleSettings>> = new Map();

connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		// Reset all cached document settings
		documentSettings.clear();
	} else {
		globalSettings = <ExampleSettings>(
			(change.settings.RiftLanguageServer || defaultSettings)
		);
	}
	// Revalidate all open text documents
	documents.all().forEach(document => validateTextDocument(document, connection));
});

export function getDocumentSettings(resource: string): Thenable<ExampleSettings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: "RiftLanguageServer"
		});
		documentSettings.set(resource, result);
	}
	return result;
}

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

// The content of a text document has changed. This event is emitted
// when the text document first opened or when its content has changed.
documents.onDidChangeContent(change => {
	getPST(change.document.uri).then((pst) => {
		console.log(pst);
	});
	validateTextDocument(change.document, connection);
});

connection.onDidChangeWatchedFiles(_change => {
	// Monitored files have change in VSCode
	connection.console.log("We received an file change event");
});

connection.onCompletion(onCompletion);
connection.onCompletionResolve(onCompletionResolve);

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();

