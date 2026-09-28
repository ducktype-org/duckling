//! The `c-bindings:` section of a generated package's manifest: how it was generated.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Default, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case", deny_unknown_fields)]
/// Everything `duck translate-c regen` needs to regenerate the package.
pub struct CBindingsRecipe {
    /// Headers to translate: a path, or a name found on the include path (`SDL3/SDL.h`).
    pub headers: Vec<String>,
    /// pkg-config packages providing compiler and linker flags.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub pkg_config: Vec<String>,
    /// Extra compiler flags, such as `-I` and `-D`.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub cflags: Vec<String>,
    /// Extra libraries: paths to `.o`, `.a` or `.so` files, or `-l`/`-L` flags.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub libraries: Vec<String>,
    /// Name globs a declaration has to match to be translated; empty means all.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub include: Vec<String>,
    /// Name globs of declarations to leave out.
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub exclude: Vec<String>,
}
