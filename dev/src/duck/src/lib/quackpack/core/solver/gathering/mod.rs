//! This module contains the utilities necessary for gathering manifests of the  packages
//! possibly occuring in the dependency resolution.
//! The two main modules are [`gatherer`] which contains the API of the module with code performing fetches
//! and [`gatherer_state`] which manages the already gathered data.
//!
//! Core description
//! ----------------
//! For each package, starting with the main one,
//! at some point a request is made to the state to return package's manifest.
//! If the manifest has not yet been gathered, we fetch it.
//! When the manifest is gathered, and a request for its manifest was made for the first time
//! or the request ordered to consider some not yet considered features of the package,
//! we make requests for the manifests of the package's dependencies.
pub mod error_surpression;
pub mod fetch_types;
pub mod gatherer;
pub mod gatherer_state;

#[cfg(test)]
mod tests;
