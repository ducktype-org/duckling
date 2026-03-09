pub mod command_ext;
pub mod env;
pub mod error;
pub mod hash;
pub mod hex;
pub mod path_ops_ext;
pub mod set_once;
pub mod toml_config;
pub mod yaml_config;

pub trait DescriptionWithAnArticle {
    fn desc_with_article(&self) -> &'static str;
}
