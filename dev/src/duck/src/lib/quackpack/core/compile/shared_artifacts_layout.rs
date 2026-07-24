//! Layout of the QuackPack's artifacts directory.
//!
//! For main package:
//! <artifacts root>
//! ├── <profile name>/
//! │   ├── *root package artifacts*/
//! │   │   ├── artifacts/ # Duckc artifacts directory
//! │   │   ├── deps.json # JSON used to communicate between QuackPack and duckc.
//! │   │   └── .duck_lock
//! │   └── *useful artifacts of the root package* # artifacts like main executable, main binary, compiled scripts, etc
//! └── .duck_lock # Global artifacts lock
//! 
//! For local dependencies:
//! <dependency artifacts root>
//! └── <hash of unit subgraph and profile>/
//!     ├── *generated artifacts of the dependency*
//!     ├── artifacts/ # Duckc artifacts directory
//!     ├── deps.json # JSON used to communicate between QuackPack and duckc.
//!     └── .duck_lock # Per dependency lock
//! 
//! For local dependencies:
//! <storage root>/pkg/.duck_build/
//! └── <hash of unit subgraph and profile>/
//!     ├── *generated artifacts of the dependency*
//!     ├── artifacts/ # Duckc artifacts directory
//!     ├── deps.json # JSON used to communicate between QuackPack and duckc.
//!     └── .duck_lock # Per dependency lock

use itertools::Itertools;

use crate::QuackResult;
use crate::quackpack::core::compile::executor::collect_packages;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::Unit;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::util::hash::sha256_string;

pub fn hash_subgraph(unit: &Unit, graph: &UnitGraph, profile: Profile) -> QuackResult<String> {
    let subgraph_string = collect_packages(unit, graph)?
        .iter().map(|pkg| pkg.id)
        .join(",");
    let to_hash = format!("{subgraph_string}-{}", profile.serialize_value());
    Ok(sha256_string(to_hash))
}
