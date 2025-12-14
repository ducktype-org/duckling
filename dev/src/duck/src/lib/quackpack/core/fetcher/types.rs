use crate::{StrId, quackpack::core::Version};

use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Package {
    pub id: StrId,
    pub version: Version,
    // @TODO: #1353 Do we want to port something safer, like <https://docs.rs/url/latest/url/>?
    pub url: StrId,
}
