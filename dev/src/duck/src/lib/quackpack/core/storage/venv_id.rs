use std::{rc::Rc, sync::Arc};

use crate::{
    StrId,
    quackpack::core::{Manifest, Package, PackageCtx},
};

/// A unique venv's identifier.
pub type VenvId = StrId;

/// Create [`VenvId`] from self.
pub trait ToVenvId {
    /// Convert self to [`VenvId`].
    fn to_venv_id(&self) -> VenvId;
}

impl<T: ToVenvId> ToVenvId for &T {
    fn to_venv_id(&self) -> VenvId {
        (*self).to_venv_id()
    }
}

impl<T: ToVenvId> ToVenvId for &mut T {
    fn to_venv_id(&self) -> VenvId {
        (**self).to_venv_id()
    }
}

impl<T: ToVenvId> ToVenvId for Box<T> {
    fn to_venv_id(&self) -> VenvId {
        (**self).to_venv_id()
    }
}

impl<T: ToVenvId> ToVenvId for Arc<T> {
    fn to_venv_id(&self) -> VenvId {
        (**self).to_venv_id()
    }
}

impl<T: ToVenvId> ToVenvId for Rc<T> {
    fn to_venv_id(&self) -> VenvId {
        (**self).to_venv_id()
    }
}

impl ToVenvId for StrId {
    fn to_venv_id(&self) -> VenvId {
        *self
    }
}

impl ToVenvId for PackageCtx<'_> {
    fn to_venv_id(&self) -> VenvId {
        self.package().to_venv_id()
    }
}

impl ToVenvId for Package {
    fn to_venv_id(&self) -> VenvId {
        self.manifest().to_venv_id()
    }
}

impl ToVenvId for Manifest {
    fn to_venv_id(&self) -> VenvId {
        // VenvId of a manifest is a package's name.
        self.name()
    }
}
