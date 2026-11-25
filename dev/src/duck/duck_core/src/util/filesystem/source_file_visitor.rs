use std::collections::VecDeque;
use std::ops::ControlFlow;
use std::path::{Path, PathBuf};

use crate::QuackResult;
use crate::util::duckling_consts::{DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT};
use crate::util::filesystem::create_dir_iterator;

/// A visitor of [`Path`].
///
/// Its implementors can be used by [`AnyPackage::try_visit_all_source_files`].
pub trait TryFileVisitor {
    /// Value reported by visitor if it stops visiting, for any reason.
    type Break;

    /// Visits one Duckling source file at `path`.
    fn visit(&mut self, path: &Path) -> QuackResult<ControlFlow<Self::Break>>;
}

impl<V: TryFileVisitor + ?Sized> TryFileVisitor for &mut V {
    type Break = V::Break;
    fn visit(&mut self, unit: &Path) -> QuackResult<ControlFlow<Self::Break>> {
        (*self).visit(unit)
    }
}

impl<V: TryFileVisitor + ?Sized> TryFileVisitor for Box<V> {
    type Break = V::Break;
    fn visit(&mut self, unit: &Path) -> QuackResult<ControlFlow<Self::Break>> {
        (**self).visit(unit)
    }
}

/// Visit all Duckling source inside a [`Path`].
///
/// If visitor returns an `Err(e)`, we short circuit to `Err(e)`
///
/// If it returns `Ok(ControlFlow::Break(b))`, we short circuit to `Ok(Some(b))`.
///
/// Otherwise (no errors + no breaks), we return `Ok(None)`
pub fn visit_duckling_source_files<V: TryFileVisitor>(
    path: &Path,
    visitor: &mut V,
) -> QuackResult<Option<V::Break>> {
    let mut paths_to_visit: VecDeque<PathBuf> = [path.to_path_buf()].into();
    while let Some(path) = paths_to_visit.pop_front() {
        if path.is_file() {
            // Check if Duckling file, if so visit it.
            if let Some(extention) = path.extension()
                && [DUCKLING_MODULE_EXT, DUCKLING_SCRIPT_EXT]
                    .iter()
                    .any(|&e| e == extention)
                && let ControlFlow::Break(b) = visitor.visit(&path)?
            {
                return Ok(Some(b));
            }
        } else {
            // If a directory, mark all its entries as to be visited.
            for entry in create_dir_iterator(&path)? {
                let entry = entry?;
                paths_to_visit.push_back(entry.path());
            }
        }
    }
    Ok(None)
}
