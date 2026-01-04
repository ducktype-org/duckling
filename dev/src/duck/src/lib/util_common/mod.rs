pub mod error;
pub mod toml_config;
pub mod yaml_config;

pub trait DescriptionWithAnArticle {
    fn desc_with_article(&self) -> &'static str;
}
