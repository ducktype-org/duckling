# Duckling Language Server

The language support for Duckling programming language.
This is the official extension provided by the Duckling team.

The extension starts the `duck_ls` binary and talks to it over stdio. Syntax highlighting,
bracket matching and the rest of the editor configuration are contributed by the extension
itself, so they keep working with the server turned off or missing.

## Configuration

The extension looks for the `duck_ls` binary at `~/.local/bin/duck_ls` by default.

Open VS Code Settings (`Ctrl+,`) and search for **Duckling**, or edit `settings.json`:

| Setting | Description | Default |
|---|---|---|
| `DucklingLanguageSupport.enable` | Run the language server. Turn off to keep only syntax highlighting. | `true` |
| `DucklingLanguageSupport.executablePath` | Path to the `duck_ls` binary. Supports `~`. | `~/.local/bin/duck_ls` |
| `DucklingLanguageSupport.trace.server` | Traces the communication with the server. | `messages` |

```json
{
    "DucklingLanguageSupport.executablePath": "/custom/path/to/duck_ls"
}
```

Both settings take effect immediately: changing either one restarts the server, and no window
reload is needed. **Duckling: Restart language server** in the command palette restarts it by
hand.

## No server

With `DucklingLanguageSupport.enable` set to `false` the extension never starts `duck_ls`.
The same happens, with a warning, when no binary is found at the configured path.
