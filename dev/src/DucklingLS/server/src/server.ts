import {
	createConnection,
	TextDocuments,
	ProposedFeatures,
	InitializeParams,
	DidChangeConfigurationNotification,
	TextDocumentSyncKind,
	InitializeResult,
	SemanticTokenTypes,
	SemanticTokenModifiers,
	FoldingRangeParams,
	FoldingRange,
	TextDocumentPositionParams,
	CompletionItem,
	Location,
	_,
	_Connection,
	FileChangeType,
	DidChangeWatchedFilesNotification
} from 'vscode-languageserver/node';

import { TextDocument } from "vscode-languageserver-textdocument";
import { handleSemanticTokensFull } from "./semanticTokens";
import { preloadKeywords } from "./preloadKeywords";
import { validateDuckling } from "./validation";
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { handleFoldingRanges } from './foldingRanges';
import { handleCompletion } from './completion';
import { handleDefinition } from './goToDefinition';

// Create a connection between the client and the server
const connection = createConnection(ProposedFeatures.all);
// Send a notification to the client that the server has started
connection.sendNotification('window/showMessage', {type: 3, message: 'DucklingLS started!'});

// Create a compiler daemon client
const compilerDaemonClient = new CompilerDaemonClient();
// Create a document manager
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;

let initPromise: Promise<void>;

// Storing LSP for documents

// Semantic tokens legend
const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

// Initial setup of the language server
connection.onInitialize(async (params: InitializeParams) => {
	const capabilities = params.capabilities;

	// Does the client support the `workspace/configuration` request?
	// If not, we fall back using global settings.
	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);
	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);

	const result: InitializeResult = {
		capabilities: {
			textDocumentSync: TextDocumentSyncKind.Incremental,
			// Tell the client that this server supports those options
			completionProvider: {
				resolveProvider: true,
				triggerCharacters: ['.']
			},
			semanticTokensProvider: {
				legend: semanticTokensLegend,
				full: true,
			},
			foldingRangeProvider: true,
			definitionProvider: true,
		}
	};


	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true
			}
		};
		await compilerDaemonClient.addWorkspace(connection, params.workspaceFolders ?? []);
	}

	// Preload keywords for autocompletion
	preloadKeywords(compilerDaemonClient, connection);
	return result;
});

connection.onInitialized(() => {
	initPromise = (async () => {
		if (hasConfigurationCapability) {
			connection.client.register(DidChangeConfigurationNotification.type, undefined);
		}
		await connection.client.register(
			DidChangeWatchedFilesNotification.type,
			{
				watchers: [
					{ globPattern: "**/*.duck" },
					{ globPattern: "**/*.dmf" }
				]
			}
		);
		if (hasWorkspaceFolderCapability) {
			// Register workspace roots so the daemon can bound upward search.
			const folders = (await connection.workspace.getWorkspaceFolders()) ?? [];
			await compilerDaemonClient.addWorkspace(connection, folders);

			connection.workspace.onDidChangeWorkspaceFolders(async _event => {
				await compilerDaemonClient.addWorkspace(connection, _event.added);
				console.log("Workspace folder change event received.");
			});
		}
	})();
});

// The example settings
interface ExampleSettings {
	maxNumberOfProblems: number;
}

// The global settings, used when the `workspace/configuration` request is not supported by the client.
const defaultSettings: ExampleSettings = { maxNumberOfProblems: 1000 };
let globalSettings: ExampleSettings = defaultSettings;

// Cache the settings of all open documents
const documentSettings: Map<string, Thenable<ExampleSettings>> = new Map();

// Listen for configuration changes
connection.onDidChangeConfiguration(change => {
	if (hasConfigurationCapability) {
		// Reset all cached document settings
		documentSettings.clear();
	} else {
		globalSettings = <ExampleSettings>(
			(change.settings.DucklingLanguageServer || defaultSettings)
		);
	}
	// Revalidate all open text documents
	documents.all().forEach(document => validateDuckling(document, connection, compilerDaemonClient));
});

export function getDocumentSettings(resource: string): Thenable<ExampleSettings> {
	if (!hasConfigurationCapability) {
		return Promise.resolve(globalSettings);
	}
	let result = documentSettings.get(resource);
	if (!result) {
		result = connection.workspace.getConfiguration({
			scopeUri: resource,
			section: "DucklingLanguageServer"
		});
		documentSettings.set(resource, result);
	}
	return result;
}

// Lazily initialise the package when a file is opened, then sync content.
documents.onDidOpen(async e => {
	await compilerDaemonClient.openFile(e.document.uri, connection);
});

// Register the handler for semantic tokens
connection.onRequest("textDocument/semanticTokens/full", (params) => 
	handleSemanticTokensFull(params, documents, compilerDaemonClient, connection)
);

connection.onRequest("duckling/restart", async () => {
    connection.window.showInformationMessage("Restarting Duckling Daemon...");
    await compilerDaemonClient.restart(connection);
	documents.all().forEach(document => compilerDaemonClient.openFile(document.uri, connection));
    connection.window.showInformationMessage("Duckling Daemon Restarted");
});

connection.onDefinition(
	async (params: TextDocumentPositionParams): Promise<Location | Location[] | null> => {
        return await handleDefinition(params, documents, compilerDaemonClient, connection);
    }
);

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

// This handler is called when the IDE detects a change in the document
documents.onDidChangeContent(change => {
	// The document has changed, so we need to update it in the compiler daemon
	compilerDaemonClient.changeContent(change.document.uri, change.document.getText(), connection).then(() => {
		// Revalidate the document
		validateDuckling(change.document, connection, compilerDaemonClient);
	});
});

connection.onDidChangeWatchedFiles(change => {
	console.log("We received an file change event");
	for (const fileEvent of change.changes) {
		switch (fileEvent.type) {

			case FileChangeType.Created:
				console.log("File created:", fileEvent.uri);
				compilerDaemonClient.newFile(fileEvent.uri, connection);
				break;

			case FileChangeType.Deleted:
				console.log("File deleted:", fileEvent.uri);
				compilerDaemonClient.deleteFile(fileEvent.uri, connection);
				documents.all().forEach(document => validateDuckling(document, connection, compilerDaemonClient));
				break;
		}
	}
});

// This handler provides the initial list of the completion items.
connection.onCompletion(
    async (_textDocumentPosition: TextDocumentPositionParams): Promise<CompletionItem[]> => {
        return await handleCompletion(_textDocumentPosition, documents, compilerDaemonClient, connection);
    }
);

// This handler resolves additional information for the item selected in the completion list.
// connection.onCompletionResolve(onCompletionResolve);

// This handler provides the folding ranges
connection.onFoldingRanges((params: FoldingRangeParams): FoldingRange[] | null => {
	return handleFoldingRanges(params, documents);
});

// Make the compiler daemon client exit when the connection exits
connection.onExit(() => {
	compilerDaemonClient.exit();
});

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();

