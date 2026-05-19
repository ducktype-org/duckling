from pathlib import Path

for path in Path(".").rglob("*.dmf"):
    text = path.read_text(encoding="utf-8")
    if not text.startswith("import core.builtins.*"):
        path.write_text("import core.builtins.*;\n\n" + text, encoding="utf-8")