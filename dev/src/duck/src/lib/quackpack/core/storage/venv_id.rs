use std::{
    ffi::{OsStr, OsString},
    fmt,
    path::Path,
    rc::Rc,
    sync::Arc,
};

use crate::{
    StrId,
    quackpack::core::{Manifest, Package, PackageCtx},
};

/// A unique venv's identifier.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum VenvId {
    Named(StrId),
}

impl VenvId {
    /// Get the name of this [`VenvId`].
    pub fn name(&self) -> StrId {
        match self {
            Self::Named(name) => *name,
        }
    }

    /// Get the static ref from [`name`](Self::name).
    pub fn static_name(&self) -> &'static str {
        self.name().as_str()
    }

    /// Check, if this [`VenvId`] corresponds to the global venv.
    pub fn is_global(&self) -> bool {
        false
    }
}

macro_rules! forward_to_asref {
    ($($type:ty)*) => {
        $(
            impl AsRef<$type> for VenvId {
                fn as_ref(&self) -> &$type {
                    self.static_name().as_ref()
                }
            }
        )*
    };
}

forward_to_asref!(OsStr str Path);

impl fmt::Display for VenvId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.name().fmt(f)
    }
}

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
        VenvId::Named(*self)
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
        self.name().to_venv_id()
    }
}

macro_rules! forward_to_strid {
    ($($type:ty)*) => {
        $(
            impl ToVenvId for $type {
                fn to_venv_id(&self) -> VenvId {
                    let id = StrId::from(self);
                    id.to_venv_id()
                }
            }
        )*
    };
}

forward_to_strid!(str String Path OsStr OsString);
