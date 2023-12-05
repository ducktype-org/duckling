# Rift Language Server

This Language Server works for ```.rift``` files. It has the following language features:
- Completions
- Diagnostics regenerated on each file change or configuration change

It also includes an End-to-End test.

## Structure

```
.
├── client // Language Client
│   ├── src
│   │   ├── test // End to End tests for Language Client / Server
│   │   └── extension.ts // Language Client entry point
├── package.json // The extension manifest.
└── server // Language Server
    └── src
        └── server.ts // Language Server entry point
```

## Running the Language Server

- Copy contents of `.vscode.template` to `.vscode` in the root folder (of the whole project).
- Press Ctrl+Shift+B to start building the project. A window will pop up asking you to select a task to run. Don't select anything for now, just ensure that a section `npm scripts` is visible in the left bottom corner of VSC.
- Run `npm install` (can be selected from the `npm scripts` modal in the left bottom corner of VSC). This installs all necessary npm modules in both the client and server folder.
- Press Ctrl+Shift+B to start compiling the client and server in `watch mode` (you might have to select it from the same modal as `npm install` if you are doing it for the first time - just run the option labeled `watch`).
- Switch to the Run and Debug View in the Sidebar (Ctrl+Shift+D).
- Select `Launch Client` from the drop down (if it is not already).
- Press ▷ to run the launch config (F5). In the new VS Code window that opens up, open a document with `.rift` extension.
- To allow logging see the section below.

## Logging

To log from the Language Server you can simply use `console.log`. 

To see the output from the Language Server you have to Run the configuration `Launch Client` and then `Attach`. The output will appear in the `Debug Console` after switching the tab to `Attach`.
