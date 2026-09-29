import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { ExtensionContext, commands, window, workspace } from "vscode";

import {
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	TransportKind
} from "vscode-languageclient/node";

/** The settings section, and the id the client derives `trace.server` from. */
const SECTION = "DucklingLanguageSupport";

const DEFAULT_EXECUTABLE_PATH = "~/.local/bin/duck_ls";

let client: LanguageClient | undefined;

/**
 * Expands a leading `~`, which VS Code does not do for configuration values.
 */
function resolveExecutablePath(configured: string): string {
	const trimmed = configured.trim();
	if (trimmed === "~") return os.homedir();
	if (trimmed.startsWith("~/")) return path.join(os.homedir(), trimmed.slice(2));
	return trimmed;
}

/**
 * Starts duck_ls and connects to it over stdio, doing nothing when the server is turned off.
 *
 * Everything the extension contributes declaratively, syntax highlighting above all, is
 * independent of this, so a missing or disabled server leaves the editor usable.
 */
async function startClient(): Promise<void> {
	if (client) return;

	const configuration = workspace.getConfiguration(SECTION);
	if (!configuration.get<boolean>("enable", true)) return;

	const executable = resolveExecutablePath(
		configuration.get<string>("executablePath", DEFAULT_EXECUTABLE_PATH)
	);

	// A bare name is looked up on the PATH, where existsSync cannot find it; let the spawn
	// report the failure in that case.
	const isPath = executable.includes(path.sep) || executable.includes("/");

	if (isPath && !fs.existsSync(executable)) {
		window.showErrorMessage(
			`Duckling: no duck_ls binary at ${executable}. Point ${SECTION}.executablePath at it, ` +
			`or set ${SECTION}.enable to false to use the extension without a language server.`
		);
		return;
	}

	const serverOptions: ServerOptions = {
		command: executable,
		transport: TransportKind.stdio
	};

	const clientOptions: LanguageClientOptions = {
		documentSelector: [{ scheme: "file", language: "duckling" }],
		synchronize: {
			// duck_ls rebuilds the package a file belongs to when one appears or disappears, so
			// it has to hear about the sources themselves.
			fileEvents: workspace.createFileSystemWatcher("**/*.{dk,dks,dl,duck}")
		}
	};

	client = new LanguageClient(SECTION, "Duckling Language Server", serverOptions, clientOptions);

	await client.start();
}

async function stopClient(): Promise<void> {
	const running = client;
	client = undefined;
	if (running) await running.stop();
}

async function restartClient(): Promise<void> {
	await stopClient();
	await startClient();
}

export async function activate(context: ExtensionContext): Promise<void> {
	context.subscriptions.push(
		commands.registerCommand("duckling.restartServer", restartClient)
	);

	// Turning the server off, or pointing it at another binary, takes effect without a reload.
	context.subscriptions.push(
		workspace.onDidChangeConfiguration(async event => {
			if (
				event.affectsConfiguration(`${SECTION}.enable`) ||
				event.affectsConfiguration(`${SECTION}.executablePath`)
			)
				await restartClient();
		})
	);

	await startClient();
}

export async function deactivate(): Promise<void> {
	await stopClient();
}
