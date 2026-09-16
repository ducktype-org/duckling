import * as path from "path";
import { workspace, ExtensionContext, commands } from "vscode";

import {
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	TransportKind
} from "vscode-languageclient/node";

let client: LanguageClient;

export function activate(context: ExtensionContext) {
	// The server is implemented in node
	const serverModule = context.asAbsolutePath(
		path.join("server", "out", "server.js")
	);

	// If the extension is launched in debug mode then the debug server options are used
	// Otherwise the run options are used
	const serverOptions: ServerOptions = {
		run: { module: serverModule, transport: TransportKind.ipc },
		debug: {
			module: serverModule,
			transport: TransportKind.ipc,
			options: { execArgv: ["--nolazy", "--inspect=6009"] }
		}
	};

	// Options to control the language client
	const clientOptions: LanguageClientOptions = {
		// Register the server for duckling documents
		documentSelector: [{ scheme: "file", language: "duckling" }],
		synchronize: {
			// Notify the server about file changes to '.clientrc files contained in the workspace
			fileEvents: workspace.createFileSystemWatcher("**/.clientrc"),
			configurationSection: 'DucklingLanguageSupport'
		},
		// Passed along with `initialize` so the server can read the duck_ls version before
		// configuration requests become legal, and report it back as serverInfo.
		initializationOptions: {
			executablePath: workspace.getConfiguration('DucklingLanguageSupport').get<string>('executablePath')
		}
	};

	// Create the language client and start the client.
	client = new LanguageClient(
		"DucklingLanguageSupport",
		"Duckling Server",
		serverOptions,
		clientOptions
	);

	// Start the client. This will also launch the server
	client.start();

	context.subscriptions.push(
		commands.registerCommand('duckling.restartServer', () => {
			client.sendRequest('duckling/restart');
		})
	);
}

// This method is called when your extension is deactivated
export function deactivate(): Thenable<void> | undefined {
	if (!client) {
		return undefined;
	}
	return client.stop();
}
