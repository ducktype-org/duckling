use std::{fmt, path::{Path, PathBuf}};

use tracing::debug;
use yaml_edit::{Mapping, YamlNode};

use crate::{QuackError, QuackResult, qp_bail, qp_bail_internal, qp_err};


pub struct YamlConfig {
    content: Mapping,
    source: Option<PathBuf>,
}

impl YamlConfig {

    /// Create an error message.
    pub fn make_location_error(&self) -> String {
        match self.source {
            Some(ref path) => format!("when parsing the configuration at `{}`", path.display()),
            None => {
                "You've encountered an internal error: when parsing the default user configuration"
                    .into()
            }
        }
    }

    fn get_location_description(&self) -> impl fmt::Display {
        self.source
            .as_ref()
            .map(|buf| buf.display())
            .unwrap_or_else(|| Path::new("<default-config>").display()) // It's dyn-hack.
    }

    fn _get(&self, key: &str) -> QuackResult<Option<YamlNode>> {
        //debug!(%key, where = %self.get_location_description());
        if key.is_empty() {
            qp_bail_internal!("empty key in `_get`")
        }
        let parts = key.split('.').collect::<Vec<_>>();
        let [ref parts @ .., last] = parts[..] else {
            unreachable!(
                "we've just asserted that the key is not empty, so split should return at least one element"
            )
        };
        let mut current: &Mapping = &self.content;
        for (i, &part) in parts.iter().enumerate() {
            if part.is_empty() {
                return Err(Self::make_empty_key_fragment_error(i, key));
            }
            let Some(next) = current.get(part) else {
                debug!(
                    table = %part,
                    chain = ?parts[0..=i],
                    "there is no table in chain",
                );
                return Ok(None);
            };
            let YamlNode::Mapping(next) = next else {
                qp_bail!(
                    "in the chain `{}` expected a table, not ",
                    parts[0..=i].join("."),
                    //next.desc_with_article(),
                )
            };
            current = &next;
        }
        Ok(current.get(last))
    }

    /// Convenient wrapper around [`_get`](Self::_get).
    fn get(&self, key: &str) -> QuackResult<Option<&YamlNode>> {
        self._get(key).with_context(|| self.make_location_error())
    }

    /// Make an error message, if i-th part of the key is empty (there are two consecutive dots).
    fn make_empty_key_fragment_error(mut i: usize, key: &str) -> QuackError {
        i += 1;
        let last_two = i % 100;
        let digit = last_two % 10;
        let decimal = last_two - digit;
        let suffix = match digit {
            1 if decimal != 10 => "st",
            2 if decimal != 10 => "nd",
            3 if decimal != 10 => "rd",
            _ => "th",
        };
        qp_err!("{i}{suffix} part of the key `{key}` is empty")
    }
}