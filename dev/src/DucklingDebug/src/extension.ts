'use strict';

import * as vscode from 'vscode';
import * as path from 'path';

export function activate(context: vscode.ExtensionContext) {

    const provider = new DuckVMConfigurationProvider();
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider('ducklingdebug', provider)
    );

    const factory = new DuckVMExecutableFactory(context);
    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('ducklingdebug', factory)
    );
3
    const trackerFactory = vscode.debug.registerDebugAdapterTrackerFactory('ducklingdebug', {
        createDebugAdapterTracker(session: vscode.DebugSession) {
            return {
                onWillReceiveMessage: m => {
                    console.log(`\n---> VS Code to DA:\n${JSON.stringify(m, null, 2)}`);
                },
                onDidSendMessage: m => {
                    console.log(`\n<--- DA to VS Code:\n${JSON.stringify(m, null, 2)}`);
                },
                onError: error => {
                    console.error(`\n!!! DA Error: ${error.message}`);
                },
                onExit: (code, signal) => {
                    console.log(`\n=== DA exited with code: ${code}, signal: ${signal} ===`);
                }
            };
        }
    });
    context.subscriptions.push(trackerFactory);

}

export function deactivate() {
    // nothing to do
}

class DuckVMConfigurationProvider implements vscode.DebugConfigurationProvider {

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

    constructor(private readonly context: vscode.ExtensionContext) {}

    createDebugAdapterDescriptor(
        session: vscode.DebugSession,
        executable: vscode.DebugAdapterExecutable | undefined
    ): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {

        if (!executable) {
            const exePath = path.join(
                this.context.extensionPath,
                'bin',
                'duck_debug_adapter'
            );

            const args: string[] = [];
            const options = {
                cwd: path.dirname(exePath),
                env: {
                    ...process.env,
                    DUCKVM_DEBUG: "1"
                }
            };

            executable = new vscode.DebugAdapterExecutable(exePath, args, options);
        }

        return executable;
    }
}
