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
	CompletionItem
} from 'vscode-languageserver/node';

import { TextDocument } from "vscode-languageserver-textdocument";
import { handleSemanticTokensFull } from "./semanticTokens";
import { getCompletionItems, onCompletionResolve, preloadKeywords } from "./completion";
import { validateDuckling } from "./validation";
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { getFoldingRanges } from './foldingRanges';
import { DucklingElement } from './lsptree/elements/elements';
import { Token } from './semanticTokensDeclarations';
require("./lsptree/elements/index");

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

// Storing LSPT for documents
const lsptCache: Map<string, JSON> = new Map();
const semanticTokensCache: Map<string, Token[]> = new Map();
const foldingCache: Map<string, FoldingRange[]> = new Map();
const completionCache: Map<string, CompletionItem[]> = new Map();

// Semantic tokens legend
const semanticTokensLegend = {
	tokenTypes: Object.values(SemanticTokenTypes),
	tokenModifiers: Object.values(SemanticTokenModifiers)
};

// Initial setup of the language server
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
		}
	};

	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true
			}
		};
	}

	// Preload keywords for autocompletion
	preloadKeywords(compilerDaemonClient, connection);
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

// Register the handler for semantic tokens
connection.onRequest("textDocument/semanticTokens/full", (params) => 
	handleSemanticTokensFull(params, documents, lsptCache, compilerDaemonClient, connection)
);

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

// Only keep settings for open documents
documents.onDidClose(e => {
	documentSettings.delete(e.document.uri);
});

// This handler is called when the IDE detects a change in the document
documents.onDidChangeContent(change => {
	// The document has changed, so we need to update it in the compiler daemon
	compilerDaemonClient.putFile(change.document.uri, change.document.getText(), connection).then(() => {
		// Get the LSPTree for the document
		compilerDaemonClient.getLSPT(change.document.uri, connection).then((LSPTree) => {
			lsptCache.set(change.document.uri, LSPTree);
			// foldingCache.delete(change.document.uri);
		});

		// Revalidate the document
		validateDuckling(change.document, connection, compilerDaemonClient);
	});
});

connection.onDidChangeWatchedFiles(_change => {
	connection.console.log("We received an file change event");
});

// This handler provides the initial list of the completion items.
connection.onCompletion((_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
	return getCompletionItems(_textDocumentPosition, documents, compilerDaemonClient, completionCache, connection);
});

// This handler resolves additional information for the item selected in the completion list.
connection.onCompletionResolve(onCompletionResolve);

// This handler provides the folding ranges
connection.onFoldingRanges((params: FoldingRangeParams): FoldingRange[] => {
	return getFoldingRanges(params, documents, compilerDaemonClient, lsptCache, connection);
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

