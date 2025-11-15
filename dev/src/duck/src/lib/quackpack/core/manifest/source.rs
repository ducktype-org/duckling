use std::path::{Path, PathBuf};

use crate::StrId;

#[derive(Debug)]
pub struct Registry {
    url: StrId,
}

impl Registry {
    pub fn new(url: StrId) -> Self {
        Self { url }
    }

    pub fn url(&self) -> StrId {
        self.url
    }
}

#[derive(Debug)]
pub struct Local {
    absolute: PathBuf,
    _entry_in_manifest: StrId,
}

impl Local {
    pub fn new(absolute: PathBuf, entry_in_manifest: StrId) -> Self {
        Self {
            absolute,
            _entry_in_manifest: entry_in_manifest,
        }
    }

    pub fn absolute(&self) -> &Path {
        &self.absolute
    }
}

#[derive(Debug)]
pub struct Git {
    url: StrId,
    branch: Option<StrId>,
    tag: Option<StrId>,
    commit: Option<StrId>,
}

impl Git {
    pub fn new(
        url: StrId,
        branch: Option<StrId>,
        tag: Option<StrId>,
        commit: Option<StrId>,
    ) -> Self {
        Self {
            url,
            branch,
            tag,
            commit,
        }
    }

    pub fn url(&self) -> StrId {
        self.url
    }

    pub fn branch(&self) -> Option<StrId> {
        self.branch
    }

    pub fn tag(&self) -> Option<StrId> {
        self.tag
    }

    pub fn commit(&self) -> Option<StrId> {
        self.commit
    }
}

#[derive(Debug)]
pub enum Source {
    Registry(Registry),
    Local(Local),
    Git(Git),
}

impl Source {
    pub fn is_git(&self) -> bool {
        matches!(self, Source::Git(..))
    }

    pub fn is_local(&self) -> bool {
        matches!(self, Source::Local(..))
    }

    pub fn is_registry(&self) -> bool {
        matches!(self, Source::Registry(..))
    }
}
