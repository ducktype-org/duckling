'use strict';

import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext) {

    const provider = new DuckVMConfigurationProvider();
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider(
            'ducklingdebug', 
            provider, 
            vscode.DebugConfigurationProviderTriggerKind.Dynamic
        )
    );

    const factory = new DuckVMExecutableFactory();
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('ducklingdebug', factory)
    );

    const duckLogChannel = vscode.window.createOutputChannel("DuckVM Debug Protocol");
    context.subscriptions.push(duckLogChannel);

    duckLogChannel.appendLine("=== DuckVM Debugger Logs Initialized ===");

    const trackerFactory = vscode.debug.registerDebugAdapterTrackerFactory('ducklingdebug', {
        createDebugAdapterTracker(session: vscode.DebugSession) {
            return {
                onWillReceiveMessage: m => {
                    console.log(`\n---> VS Code to DA:\n${JSON.stringify(m, null, 2)}`);
                    duckLogChannel.appendLine(`\n---> VS Code to DA:\n${JSON.stringify(m, null, 2)}`);
                },
                onDidSendMessage: m => {
                    console.log(`\n<--- DA to VS Code:\n${JSON.stringify(m, null, 2)}`);
                    duckLogChannel.appendLine(`\n<--- DA to VS Code:\n${JSON.stringify(m, null, 2)}`);
                },
                onError: error => {
                    console.error(`\n!!! DA Error: ${error.message}`);
                    duckLogChannel.appendLine(`\n!!! DA Error: ${error.message}`);
                },
                onExit: (code, signal) => {
                    console.log(`\n=== DA exited with code: ${code}, signal: ${signal} ===`);
                    duckLogChannel.appendLine(`\n=== DA exited with code: ${code}, signal: ${signal} ===`);
                }
            };
        }
    });

    let runHandler = vscode.commands.registerCommand('ducklingdebug.runFile', (uri: vscode.Uri) => {
        const targetUri = uri || vscode.window.activeTextEditor?.document.uri;
        if (!targetUri) return;

        vscode.debug.startDebugging(undefined, {
            type: "ducklingdebug",
            name: "Duckling: Run Active File",
            request: "launch",
            program: targetUri.fsPath,
            noDebug: true
        });
    });

    let debugHandler = vscode.commands.registerCommand('ducklingdebug.debugFile', (uri: vscode.Uri) => {
        const targetUri = uri || vscode.window.activeTextEditor?.document.uri;
        if (!targetUri) return;

        vscode.debug.startDebugging(undefined, {
            type: "ducklingdebug",
            name: "Duckling: Debug Active File",
            request: "launch",
            program: targetUri.fsPath
        });
    });

    context.subscriptions.push(runHandler, debugHandler, trackerFactory);
}

export function deactivate() {
    // nothing to do
}
class DuckVMConfigurationProvider implements vscode.DebugConfigurationProvider {

   provideDebugConfigurations(
        folder: vscode.WorkspaceFolder | undefined, 
        token?: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DebugConfiguration[]> {
        return [
            {
                type: 'ducklingdebug',
                request: 'launch',
                name: 'Duckling: Launch Current File',
                program: '${file}'
            }
        ];
    }

    resolveDebugConfiguration(
        folder: vscode.WorkspaceFolder | undefined,
        config: vscode.DebugConfiguration
    ): vscode.ProviderResult<vscode.DebugConfiguration> {

        if (!config.type && !config.request && !config.name) {
            const editor = vscode.window.activeTextEditor;
            if (editor) {
                config.type = 'ducklingdebug';
                config.name = 'Launch DuckVM';
                config.request = 'launch';
                config.program = '${file}';
            }
        }

        if (!config.program) {
            vscode.window.showErrorMessage("DuckVM: No program specified to debug.");
            return undefined;
        }

        return config;
    }
}

class DuckVMExecutableFactory implements vscode.DebugAdapterDescriptorFactory {

    constructor() {}

    createDebugAdapterDescriptor(
        session: vscode.DebugSession,
        executable: vscode.DebugAdapterExecutable | undefined
    ): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {

        return executable;
    }
}
