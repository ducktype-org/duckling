'use strict';

import * as cp from 'child_process';
import * as path from 'path';
import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext) {

    const provider = new DuckVMConfigurationProvider(context);
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

    constructor(private context: vscode.ExtensionContext) {}

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
         return config;
     }
 
     resolveDebugConfigurationWithSubstitutedVariables(
         folder: vscode.WorkspaceFolder | undefined,
         config: vscode.DebugConfiguration
     ): vscode.ProviderResult<vscode.DebugConfiguration> {
 
         if (!config.program) {
             vscode.window.showErrorMessage("DuckVM: No program specified to debug.");
             return undefined;
         }
 
         const rawFilePath = config.program;
         console.log(rawFilePath);
         const parsedPath = path.parse(rawFilePath);
 
         if (parsedPath.ext === '.dk') {
            console.log("IN");
            const packageName = parsedPath.name;
            const workingDir = parsedPath.dir;
 
            const compilerPath = path.resolve(this.context.extensionPath, '../../build/bin/duckc');
            console.log(compilerPath);

 
             const compileCmd = `"${compilerPath}" compile_package -n main "${packageName}.dk" --dvm-backend`;
 
             try {
                 cp.execSync(compileCmd, { cwd: workingDir });
              } catch (error: any) {
                 const errorLog = error.stderr ? error.stderr.toString() : error.message;
                 vscode.window.showErrorMessage(`Duckling compilation failed:\n${errorLog}`);
                 return undefined;
             }
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
