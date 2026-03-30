# Duckling Language Server

The language support for Duckling programming language.
It is the official extension provided by the Duckling team.

## Configuration

The extension looks for the `duck_ls` binary at `~/.local/bin/duck_ls` by default.

To use a different path, open VS Code Settings (`Ctrl+,`), search for **Duckling**, and set:

| Setting | Description | Default |
|---|---|---|
| `DucklingLanguageSupport.executablePath` | Path to the `duck_ls` binary. Supports `~`. | `~/.local/bin/duck_ls` |

Or add this to your `settings.json`:

```json
{
    "DucklingLanguageSupport.executablePath": "/custom/path/to/duck_ls"
}
```
