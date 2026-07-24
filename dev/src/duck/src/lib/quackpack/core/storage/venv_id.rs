use std::ffi::{OsStr, OsString};
use std::fmt;
use std::path::Path;
use std::rc::Rc;
use std::sync::Arc;

use crate::StrId;
use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::{AnyPackage, FrontMatterScript, Manifest, Package, PackageContext};
use crate::util::hash::sha256_string;

/// A unique venv's identifier.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum VenvId {
    Package(StrId),
    Script {
        script_name: StrId,
        venv_name: StrId,
    },
    Global,
}

impl VenvId {
    /// Get the name of this [`VenvId`].
    pub fn name(&self) -> StrId {
        match self {
            Self::Package(name) => *name,
            Self::Global => StrId::new(DuckHome::GLOBAL_PACKAGE_NAME),
            Self::Script {
                script_name: _,
                venv_name,
            } => *venv_name,
        }
    }

    /// Get the static ref from [`name`](Self::name).
    pub fn static_name(&self) -> &'static str {
        self.name().as_str()
    }

    /// Check, if this [`VenvId`] corresponds to the global venv.
    pub fn is_global(&self) -> bool {
        matches!(self, Self::Global)
    }
}

macro_rules! forward_to_as_ref {
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

forward_to_as_ref!(OsStr str Path);

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

impl ToVenvId for VenvId {
    fn to_venv_id(&self) -> VenvId {
        *self
    }
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
        if self == DuckHome::GLOBAL_PACKAGE_NAME {
            VenvId::Global
        } else {
            VenvId::Package(*self)
        }
    }
}

impl ToVenvId for PackageContext<'_> {
    fn to_venv_id(&self) -> VenvId {
        self.package().to_venv_id()
    }
}

impl ToVenvId for AnyPackage {
    fn to_venv_id(&self) -> VenvId {
        match self {
            Self::Package(package) => package.to_venv_id(),
            Self::Frontmatter(front_matter_script) => front_matter_script.to_venv_id(),
        }
    }
}

impl ToVenvId for Package {
    fn to_venv_id(&self) -> VenvId {
        self.manifest().to_venv_id()
    }
}

impl ToVenvId for FrontMatterScript {
    fn to_venv_id(&self) -> VenvId {
        let hash = sha256_string(self.script_file().as_os_str().as_encoded_bytes());
        let id = format!("{}-{hash}", self.manifest().name());
        VenvId::Script {
            script_name: self.manifest().name(),
            venv_name: id.into(),
        }
    }
}

impl ToVenvId for Manifest {
    fn to_venv_id(&self) -> VenvId {
        if self.is_global() {
            VenvId::Global
        } else {
            // VenvId of a manifest is a package's name.
            self.name().to_venv_id()
        }
    }
}

macro_rules! forward_to_str_id {
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

forward_to_str_id!(str String Path OsStr OsString);

impl ToVenvId for &str {
    fn to_venv_id(&self) -> VenvId {
        StrId::from(*self).to_venv_id()
    }
}

impl ToVenvId for &OsStr {
    fn to_venv_id(&self) -> VenvId {
        StrId::from(*self).to_venv_id()
    }
}
