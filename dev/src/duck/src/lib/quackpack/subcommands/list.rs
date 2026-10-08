// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::cmp::Ordering;
use std::path::PathBuf;

use chrono::{DateTime, Utc};

use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::storage::venv::Venv;
use crate::quackpack::core::storage::{display_venv_info, list_venvs};
use crate::util::Pluralize;
use crate::{DuckContext, QuackResult, QuackResultContext};

/// Options for the list operation.
pub struct ListOptions<'duck> {
    pub ctx: &'duck DuckContext,
    /// How to order the output.
    pub output_ordering: VenvOrderings,
    /// Whether to reverse the order specified above.
    pub reverse_order: bool,
    /// Path to the storage from which we want to list the venvs.
    pub storage_path: PathBuf,
}

/// Enum for possible orderings of venvs.
#[derive(Debug, Clone, Copy)]
pub enum VenvOrderings {
    Name,
    Access,
    Modification,
}

impl VenvOrderings {
    /// Return the correct function pointer for comparing venvs.
    pub fn comparator(self) -> fn(&Venv, &Venv) -> Ordering {
        match self {
            VenvOrderings::Name => Venv::compare_name,
            VenvOrderings::Access => Venv::compare_access,
            VenvOrderings::Modification => Venv::compare_modification,
        }
    }
}

impl Venv {
    /// Compare venvs by name alphabetically.
    fn compare_name(&self, other: &Self) -> Ordering {
        self.id().name().cmp(&other.id().name())
    }

    /// Compare venvs by last_access ascendingly.
    fn compare_access(&self, other: &Self) -> Ordering {
        self.data().last_access().cmp(&other.data().last_access())
    }

    /// Compare venvs by last_synchronization ascendingly.
    fn compare_modification(&self, other: &Self) -> Ordering {
        self.data()
            .last_synchronization()
            .cmp(&other.data().last_synchronization())
    }
}

/// List the venvs given options.
pub fn list(opts: ListOptions<'_>) -> QuackResult<()> {
    let ListOptions {
        ctx,
        output_ordering,
        reverse_order,
        storage_path,
    } = opts;
    let storage = Storage::new(storage_path);
    let venvs_list: Vec<(Venv, DateTime<Utc>)> = list_venvs(storage.root(), ctx)
        .context("when listing the venvs")?
        .into_values()
        .collect();
    // This list operation counts as access to the venv, modyfying the last_access to now.
    // To display a meaningful value of the last_access, we substitute the last_access field
    // with the last_access prior to this current list operation.
    let mut venvs_list: Vec<Venv> = venvs_list
        .into_iter()
        .map(|(mut venv, time)| {
            venv.data_mut().set_last_access(time);
            venv
        })
        .collect();
    venvs_list.sort_by(output_ordering.comparator());
    if reverse_order {
        venvs_list.reverse();
    }

    ctx.print(format!(
        "Found {} venv{}",
        venvs_list.len(),
        venvs_list.s_if_plural(),
    ))?;

    for venv in venvs_list {
        display_venv_info(ctx, venv)?;
    }
    Ok(())
}
