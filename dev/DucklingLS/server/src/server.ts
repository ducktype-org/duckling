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
import { validateDuckling } from "./validation"; // Import the validation function
import { CompilerDaemonClient } from "./compilerDaemonClient";
import { getFoldingRanges } from './foldingRanges';
import { DucklingElement } from './lsptree/elements/elements';
import { Token } from './semanticTokensDeclarations';
require("./lsptree/elements/index");


const connection = createConnection(ProposedFeatures.all);
connection.sendNotification('window/showMessage', {type: 3, message: 'DucklingLS started!'});

const compilerDaemonClient = new CompilerDaemonClient();
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let hasDiagnosticRelatedInformationCapability = false;

// Storing PST for documents
const lsptCache: Map<string, DucklingElement | null> = new Map();
const semanticTokensCache: Map<string, Token[]> = new Map();

// Semantic tokens legend
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

connection.onRequest("textDocument/semanticTokens/full", (params) => 
	handleSemanticTokensFull(params, documents, lsptCache, semanticTokensCache, compilerDaemonClient, connection)
);

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

documents.onDidChangeContent(change => {
	compilerDaemonClient.putFile(change.document.uri, change.document.getText(), connection).then(() => {
		compilerDaemonClient.getLSPT(change.document.uri, connection).then((LSPTree) => {
			lsptCache.set(change.document.uri, LSPTree);
			console.log(LSPTree);
		});

		validateDuckling(change.document, connection, compilerDaemonClient);
	});
});


connection.onDidChangeWatchedFiles(_change => {
	connection.console.log("We received an file change event");
});

connection.onCompletion((_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] => {
	return getCompletionItems(_textDocumentPosition, documents, lsptCache);
});
connection.onCompletionResolve(onCompletionResolve);

connection.onFoldingRanges((params: FoldingRangeParams): FoldingRange[] | null => {
	return getFoldingRanges(params, documents, semanticTokensCache, lsptCache);
});

connection.onExit(() => {
	compilerDaemonClient.exit();
});

// Make the text document manager listen on the connection
// for open, change and close text document events
documents.listen(connection);

// Listen on the connection
connection.listen();

