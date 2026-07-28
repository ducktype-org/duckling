//! This module contains the utilities necessary for gathering manifests of the  packages
//! possibly occurring in the dependency resolution.
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
//!
//! Note(terminology):
//! ------------------
//! 1. a *request* signifies a need to read the manifest of a given package (pinned request) or the
//!    manifests of all the packages with a given source and name satisfying some versions constraints (not pinned request),
//! 2. a *fetch* is a process of obtaining manifest(s) for the first time, for example from Ducknest,
//! 3. to satisfy a *request*, a *fetch* may be made, this usually happens for the first *request* referencing a specific source and name/package.
//! 4. requests are identified by a pair (source, name), which is called the request's id.
//!
//! Requests usually lead to more requests.
//! Generally after getting a request for a manifest of some package,
//! if that request changed anything, we make requests for all of the package's dependencies.
//!
//! Pinned & Not Pinned vs Registry, Git & Local:
//! ---------------------
//! 1. Git and Local requests are always not pinned, though they can only return one manifest.
//! 2. Registry requests can be either pinned or not pinned.
//!
//! Errors:
//! -------
//! Since in the gathering process we might discover errors in another packages and not our,
//! all the errors are collected during the gathering process and depending on the user,
//! either printed as warnings or reported as errors.
//! [`QuackResult`](crate::QuackResult) is generally only used for internal errors.
pub mod fetch_types;
pub mod gatherer;
pub mod gatherer_state;

#[cfg(test)]
mod tests;
