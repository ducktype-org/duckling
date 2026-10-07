// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Utilities for validating a package name.

use thiserror::Error;

/// __Sorted__ list of std Duckling packages.
const DUCKLING_STD_PACKAGES: &[&str] = &["core", "std"];

/// __Sorted__ list of builtin Duckling keywords.
const DUCKLING_KEYWORDS: &[&str] = &[
    "Array",
    "Dict",
    "List",
    "Set",
    "and",
    "assert",
    "block",
    "bool",
    "box",
    "break",
    "case",
    "catch",
    "char",
    "compile_assert",
    "const",
    "continue",
    "copy",
    "copyof",
    "cptr",
    "debug",
    "defer",
    "destroy",
    "else",
    "export",
    "extern",
    "f128",
    "f16",
    "f32",
    "f64",
    "f80",
    "false",
    "for",
    "fun",
    "fundecl",
    "global",
    "hides",
    "i128",
    "i16",
    "i32",
    "i64",
    "i8",
    "if",
    "implements",
    "import",
    "in",
    "lamba",
    "lambda",
    "let",
    "loop",
    "manyptr",
    "match",
    "move",
    "namespace",
    "none",
    "not",
    "or",
    "pattern",
    "private",
    "protected",
    "ptr",
    "public",
    "redo",
    "ref",
    "refof",
    "restart",
    "return",
    "self",
    "sizeof",
    "slice",
    "str",
    "switch",
    "template",
    "then",
    "throw",
    "try",
    "true",
    "type",
    "u128",
    "u16",
    "u32",
    "u64",
    "u8",
    "using",
    "var",
    "while",
    "with",
    "xor",
];

#[derive(Debug, Error, Clone, Eq, PartialEq)]
pub enum PackageNameError {
    #[error("package name is empty")]
    Empty,
    #[error("package name `{name}` starts with an illegal character `{char}`")]
    StartsWithIllegalCharacter { name: String, char: char },
    #[error("package name `{name}` contains an illegal character `{char}`")]
    ContainsIllegalCharacter { name: String, char: char },
}

/// Validate a package name `name`.
pub fn validate_package_name(name: &str) -> Result<(), PackageNameError> {
    let Some(first) = name.chars().next() else {
        return Err(PackageNameError::Empty);
    };
    if !is_valid_first_package_character(first) {
        return Err(PackageNameError::StartsWithIllegalCharacter {
            name: name.to_string(),
            char: first,
        });
    }
    for char in name.chars() {
        if !is_valid_package_character(char) {
            return Err(PackageNameError::ContainsIllegalCharacter {
                name: name.to_string(),
                char,
            });
        }
    }
    Ok(())
}

/// Normalise previously validated package name into a duckling identifier.
pub fn normalise_package_name(name: &str) -> String {
    // To become a valid duckling identifier, we need to convert all illegal identifier characters
    // present in a package name into valid ones. Because the only one is a dash (`-`), we replace
    // it with an underscore (`_`).
    name.replace('-', "_")
}

/// Check if a name is a Duckling std package name.
pub fn is_duckling_std_name(name: &str) -> bool {
    DUCKLING_STD_PACKAGES.contains(&name)
}

/// Check if a name is a Duckling keyword.
pub fn is_duckling_keyword(name: &str) -> bool {
    DUCKLING_KEYWORDS.contains(&name)
}

/// Check if a given char is a valid package name character.
/// Allowed values are:
/// - lowercase ASCII letters,
/// - uppercase ASCII letters,
/// - ASCII digits,
/// - `-` ASCII char,
/// - `_` ASCII char.
fn is_valid_package_character(char: char) -> bool {
    char.is_ascii_alphanumeric() || char == '-' || char == '_'
}

/// Check if a given char is a valid __first__ package name character.
/// Allowed values are:
/// - lowercase ASCII letters,
/// - uppercase ASCII letters,
/// - `_` ASCII char.
fn is_valid_first_package_character(char: char) -> bool {
    is_valid_package_character(char) && !char.is_ascii_digit() && char != '-'
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_package_names() {
        let valid_package_names = [
            "a",
            "a-",
            "_a",
            "Windows",
            "_a",
            "a-b",
            "foo-bar",
            "foo_Bar",
            "f4bar",
            "xd",
            "dXX",
            "_",
            "b3-aWagagkahgjka-_",
        ];
        for name in valid_package_names {
            validate_package_name(name).unwrap();
        }
    }

    #[test]
    fn invalid_package_names() {
        let test_errors = [
            ("", PackageNameError::Empty),
            (
                "-",
                PackageNameError::StartsWithIllegalCharacter {
                    name: "-".to_string(),
                    char: '-',
                },
            ),
            (
                "3",
                PackageNameError::StartsWithIllegalCharacter {
                    name: "3".to_string(),
                    char: '3',
                },
            ),
            (
                "abą",
                PackageNameError::ContainsIllegalCharacter {
                    name: "abą".to_string(),
                    char: 'ą',
                },
            ),
            (
                "ab(",
                PackageNameError::ContainsIllegalCharacter {
                    name: "ab(".to_string(),
                    char: '(',
                },
            ),
            (
                "a!",
                PackageNameError::ContainsIllegalCharacter {
                    name: "a!".to_string(),
                    char: '!',
                },
            ),
        ];
        for (name, err) in test_errors {
            assert_eq!(err, validate_package_name(name).unwrap_err());
        }
    }
}
