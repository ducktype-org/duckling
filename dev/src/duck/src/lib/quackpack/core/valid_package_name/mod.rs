//! Utilities for validating a package name.

use thiserror::Error;

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
