use std::path::PathBuf;

use url::Url;

use crate::{QuackResult, qp_internal};

pub trait ToPathBuf {
    fn to_path_buf(&self) -> QuackResult<PathBuf>;
}

impl ToPathBuf for Url {
    fn to_path_buf(&self) -> QuackResult<PathBuf> {
        self.to_file_path()
            .map_err(|_| qp_internal!("failed to convert url into a path: {self:?}"))
    }
}
