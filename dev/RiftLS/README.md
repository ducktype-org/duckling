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
- Go to `/dev/RiftLS` folder and run `npm install`. This installs all necessary npm modules in both the client and server folder.
- Press Ctrl+Shift+B to start building the project. The project should automatically compile in watch mode (new terminal named `npm: watch` should appear - you can check in the bottom right). If a window pops up asking you to select a task to run, select `npm: watch` - this will start the compiler in watch mode. Alternatively you can try to skip compiling it yourself and just run the launch config `Launch Client` (see below) - it should start the compiler in watch mode as a part of the launch config.
- Check if section `npm scripts` is visible in the bottom left corner of VSC (if you can't see it check the VSC explorer options - three dots in the top right corner of the explorer and select `npm scripts` if it's not checked). Not having this section is not a blocker but it's useful to have.
- Switch to the Run and Debug View in the Sidebar (Ctrl+Shift+D).
- Select `Launch Client` from the drop down (if it is not already).
- Press ▷ to run the launch config (F5). In the new VS Code window that opens up, open a document with `.rift` extension.
- To allow logging see the section below.

## Logging

To log from the Language Server you can simply use `console.log`. 

To see the output from the Language Server you have to Run the configuration `Launch Client` and then `Attach`. The output will appear in the `Debug Console` after switching the tab to `Attach`.
