# custom Pygments highlighting:
from pygments.lexer import RegexLexer, words
from pygments import token


class DucklingLexer(RegexLexer):
    """
    Lexer for the Duckling programming language, syntax version: 2.0
    """

    name = "duckling"
    url = "https://ducktype.org/"
    filenames = ["*.duck", "*.dmf"]
    aliases = ["duck", "du"]

    attributes = (words(("@in", "@out"), suffix=r"\b"), token.Name.Builtin)
    keywords = (
        words(
            (
                "type",
                "block",
                "class",
                "interface",
                "fun",
                "pure",
                "lambda",
                "return",
                "while",
                "for",
                "loop",
                "if",
                "else",
                "break",
                "continue",
                "in",
                "as",
                "namespace",
                "exec",
                "use",
                "let",
                "var",
                "const",
                "debug",
                "test",
                "alias",
                "using",
                "template",
                "generic",
                "macro",
                "expand",
                "requires",
                "self",
            ),
            suffix=r"\b",
        ),
        token.Keyword,
    )
    types = (
        words(
            (
                "i8",
                "i16",
                "i32",
                "i64",
                "i8",
                "u16",
                "u32",
                "u64",
                "f32",
                "f64",
                "bool",
            ),
            suffix=r"\b",
        ),
        token.Keyword.Type,
    )
    value_keywords = (
        words(("true", "false", "null"), suffix=r"\b"),
        token.Keyword.Constant,
    )

    token_name = r"[_a-zA-Z][_a-zA-Z0-9]*"

    tokens = {
        "root": [
            (r"\n", token.Whitespace),
            (r"\s+", token.Whitespace),
            attributes,
            keywords,
            types,
            value_keywords,
            (r'"', token.String, "string"),
            (r"\#\{", token.Comment.Multiline, "comment"),
            (r"\#.*?$", token.Comment.Singleline),
            (r"[0-9][0-9\.]*", token.Number),
            (token_name, token.Name),
            (r"[+-\/*%=><|^*&@]", token.Operator),
            (r":=", token.Operator),
            (r"\.\.\.", token.Other),
            (r"[;:,\(\)\{\}\[\]]", token.Punctuation),
        ],
        "comment": [
            (r"[^\#\{\}]+", token.Comment.Multiline),  # Do not capture any of `#{}`
            (r"\#\{", token.Comment.Multiline),  # `#{` -> multiline
            (r"\#\}", token.Comment.Multiline, "#pop"),  # `#}` -> end multiline
            (r"[\#\}\{]", token.Comment.Multiline),  # Not matched -> multiline
        ],
        "string": [
            (r'[^"\\]+', token.String),
            (r"\\.", token.String.Escape),
            ('"', token.String, "#pop"),
        ],
    }
