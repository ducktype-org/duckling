use std::str::FromStr;

use dialoguer::Input;
use regex::Regex;

use crate::{QuackResult, StrId};

/// Prompt user until the result can be interpreted as an instance of `T`.
pub fn with_default<T: FromStr + ToString>(prompt: StrId, default: T) -> QuackResult<T> {
    loop {
        let input: String = Input::new()
            .default(default.to_string())
            .with_prompt(prompt)
            .interact_text()?;
        if let Ok(new_result) = T::from_str(&input) {
            return Ok(new_result);
        }
    }
}

/// Prompt user to get a `String`, with a default option supplied.
pub fn string_with_default(prompt: StrId, default: StrId) -> QuackResult<String> {
    let input: String = Input::new()
        .default(default.to_string())
        .with_prompt(prompt)
        .interact_text()?;
    Ok(input)
}

/// Prompt user for a `String` until the result satisfies a regex.
pub fn string_no_default_with_regex(
    prompt: StrId,
    regex: Regex
) -> QuackResult<String> {
    loop {
        let input: String = Input::new()
            .with_prompt(prompt)
            .interact_text()?;
        if regex.is_match(&input) {
            return Ok(input)
        }
    }
}
