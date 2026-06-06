use std::path::{Path, PathBuf};

use url::Url;

use crate::{QuackResult, QuackResultContext, qp_err};

pub trait ToUrl {
    fn to_url(&self) -> QuackResult<Url>;
}

impl ToUrl for str {
    fn to_url(&self) -> QuackResult<Url> {
        Url::parse(self).with_context(|| format!("`{self}` is not a valid url"))
    }
}

impl ToUrl for Path {
    fn to_url(&self) -> QuackResult<Url> {
        Url::from_file_path(self)
            .map_err(|_| qp_err!("failed to turn `{}` into a url", self.display()))
    }
}

impl ToUrl for PathBuf {
    fn to_url(&self) -> QuackResult<Url> {
        self.as_path().to_url()
    }
}
