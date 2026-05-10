//! Common quackpack and duck functions.
pub mod command_ext;
pub mod env;
pub mod error;
pub mod extract;
pub mod file_locks;
pub mod hash;
pub mod hex;
pub mod path_ops_ext;
pub mod set_once;
pub mod yaml_config;

#[cfg(test)]
pub mod test_utils;

/// English localization helper trait.
pub trait DescriptionWithAnArticle {
    /// Return a valid description with an appropriate article.
    fn desc_with_article(&self) -> &'static str;
}
