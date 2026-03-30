## Dependencies

To correctly package this extension node version >=20 is required.

NVM is an easy way to manage node versions that also allows to install vsce in user files.


## Structure

```
.
├── client // Language Client
│   ├── src
│   │   └── extension.ts // Language Client entry point
├── package.json // The extension manifest.
└── server // Language Server
    └── src
        └── server.ts // Language Server entry point
```

## Running the Language Server

### Running the language server for testers

1. Go to the `/dev/src/DucklingLS` directory.
2. Compile and install `duck_ls` to `~/.local/bin/` using the `./comp-copy.py <name-of-the-build-dir>` command.
3. Run `npm install` to install the dependencies.
4. Run `npm run compile` to compile the typescript to the js in the `out` directories.
5. 
   - [on more recent vscode version] Run vscode command: `Developer: Install Extension from Location` with this directory location. 
   - [on old versions of vscode]  Run `ln -s $(pwd) ~/.vscode/extensions/duckling-support` to make a link from the vs code extensions
directory to this directory, which is a root folder of the extension. 
6. Restart the vscode. 
7. Check in the `extensions` vs code tab `DucklingLS` extension.

### Running the language server for developers

- Copy contents of `.vscode.template` to `.vscode` in the root folder (of the whole project).
- Run `./comp-copy.py <build-dir>` to install `duck_ls` to `~/.local/bin/`, or copy it manually and set `DucklingLanguageSupport.executablePath` in your VS Code settings.
- Go to `/dev/DucklingLS` folder and run `npm install`. This installs all necessary npm modules in both the client and server folder.
- Press Ctrl+Shift+B to start building the project. The project should automatically compile in watch mode (new terminal named `npm: watch` should appear - you can check in the bottom right). If a window pops up asking you to select a task to run, select `npm: watch` - this will start the compiler in watch mode. Alternatively you can try to skip compiling it yourself and just run the launch config `Launch Client` (see below) - it should start the compiler in watch mode as a part of the launch config.
- Check if section `npm scripts` is visible in the bottom left corner of VSC (if you can't see it check the VSC explorer options - three dots in the top right corner of the explorer and select `npm scripts` if it's not checked). Not having this section is not a blocker but it's useful to have.
- Switch to the Run and Debug View in the Sidebar (Ctrl+Shift+D).
- Select `Launch Client` from the drop down (if it is not already).
- Press ▷ to run the launch config (F5). In the new VS Code window that opens up, open a document with `.duckling` extension.
- To allow logging see the section below.

## Logging

To log from the Language Server you can simply use `console.log`. 

To see the output from the Language Server you have to Run the configuration `Launch Client` and then `Attach`. The output will appear in the `Debug Console` after switching the tab to `Attach`.

## Packaging The Extension

Its best to follow instructions from https://code.visualstudio.com/api/working-with-extensions/publishing-extension#create-a-publisher

A quick summary:

To generate a `.vsix` file for your extension, follow these steps:

1. Install `vsce` globally using npm. This tool is required for packaging and publishing extensions.

    ```bash
    npm install -g @vscode/vsce
    ```

2. Package your extension. This command generates a `.vsix` file in your current directory. You can optionally specify a name for the output file using `--out`.

    ```bash
    vsce package
    # Optionally, specify the output file name
    vsce package --out my-extension.vsix
    ```

## Publishing Your Extension

After packaging your extension, you can publish it to the Visual Studio Code Marketplace:

1. Run the following command to publish your extension. Ensure you're logged in to `vsce` with your publisher account.

    ```bash
    vsce publish
    ```
