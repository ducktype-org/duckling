//! Progress bars utilities for displaying different progress bars consistently.
//! Implementation follows Manager/Model -> View pattern
//! (similar to bloc[^bloc][^flutter], where BLoC emits new states, and UI just listens and renders
//! a given state).
//!
//! [^bloc]: <https://bloclibrary.dev/>
//! [^flutter]: <https://pub.dev/packages/flutter_bloc>

use std::collections::{BTreeMap, HashSet};
use std::sync::RwLock;

use indicatif::{ProgressBar, ProgressDrawTarget, ProgressStyle};
use itertools::Itertools;
use tracing::debug;

use crate::StrId;
use crate::duck::util::terminal::Terminal;
use crate::quackpack::util::PANIC_MESSAGE;

const DEFAULT_REFRESH_RATE_HZ: u8 = 20;

#[derive(Debug)]
#[allow(unused)] // We want to switch between them easier, not export them.
// Common progress bars styles, for an easier configuration.
enum PrefixChars {
    // Progress bar like `[####   ]`.
    Blocks,
    // Progress bar like `[===>   ]`.
    Arrow,
    // Progress bar like `[####---]`.
    Pacman,
    // For ArchLinux users: add `ILoveCandy` under `[options]` section in `/etc/pacman.conf` :^).
    PacmanCandy,
}

impl PrefixChars {
    /// Get [`indicatif`] friendly string for displaying a progress.
    fn as_indicatif_progress_chars(&self) -> &'static str {
        // These should match https://docs.rs/indicatif/latest/indicatif/style/struct.ProgressStyle.html#method.progress_chars.
        match self {
            PrefixChars::Blocks => "## ",
            PrefixChars::Arrow => "=> ",
            PrefixChars::Pacman => "##-",
            // This doesn't work the best...
            PrefixChars::PacmanCandy => "-Co",
        }
    }
}

#[derive(Debug)]
/// Manager for displaying currently downloading packages.
pub struct DownloadingPackagesProgressBarManager {
    bar: DownloadingPackagesProgressBar,
    to_download: HashSet<StrId>,
    state: RwLock<DownloadablePackagesState>,
}

#[derive(Debug, Default)]
struct DownloadablePackagesState {
    next_id: usize,
    currently_downloading: BTreeMap<usize, StrId>,
    reverse_map: BTreeMap<StrId, usize>,
}

impl DownloadablePackagesState {
    fn format(&self) -> String {
        self.currently_downloading.values().copied().join(", ")
    }
}

impl DownloadingPackagesProgressBarManager {
    pub fn new(term: &Terminal, to_download: Vec<StrId>) -> Self {
        Self {
            bar: DownloadingPackagesProgressBar::new(term, to_download.len() as u64),
            to_download: to_download.into_iter().collect(),
            state: Default::default(),
        }
    }

    pub fn start_download_of(&self, pkg: StrId) {
        if !self.to_download.contains(&pkg) {
            debug!(
                "can't start a download of `{pkg}`, because it was not declared as a package to download"
            );
            return;
        }
        let mut state = self.state.write().expect(PANIC_MESSAGE);
        if state.reverse_map.contains_key(&pkg) {
            debug!("download of package `{pkg}` has already started");
            return;
        }
        let id = state.next_id;
        state.next_id += 1;

        state.currently_downloading.insert(id, pkg);
        state.reverse_map.insert(pkg, id);

        let formatted = state.format();
        debug!("package `{pkg}` has been assigned id `{id}`");
        // We care about concurrency: we want to set new message under the lock, to avoid a following scenario:
        // 1. We want to add a package.
        // 2. Some other threads remove its package and finished everything except setting a new message.
        // 3. We complete adding a new package.
        // 4. Removing thread finished, and our package disappears from the progress bar.
        self.bar.bar.set_message(formatted);
        drop(state);
    }

    pub fn finish_download_of(&self, pkg: StrId) {
        if !self.to_download.contains(&pkg) {
            debug!(
                "can't finish a download of `{pkg}`, because it was not declared as a package to download"
            );
            return;
        }
        let mut state = self.state.write().expect(PANIC_MESSAGE);

        let Some(id) = state.reverse_map.remove(&pkg) else {
            debug!("download of package `{pkg}` hasn't started");
            return;
        };
        state.currently_downloading.remove(&id);
        let formatted = state.format();
        debug!("package `{pkg}` has deduced its id `{id}`");
        // We want to set message under the lock, to avoid a following scenario:
        // 1. We want to remove a package.
        // 2. We're up to this point, we've created `formatted`.
        // 3. Some other threads wants to *start* a new download.
        // 4. It creates its own `formatted` and sets it.
        // 5. We override it here, causing a new package to disappear.
        // However, we can tick without a lock: drawing backend takes care of concurrency/parallelism,
        // and we tick only here: user doesn't care, if ticks came from wrong threads, he only cares that a tick has happened.
        self.bar.bar.set_message(formatted);
        drop(state);
        self.bar.bar.inc(1);
    }
}

#[derive(Debug)]
/// Progress bar for displaying currently downloading packages.
struct DownloadingPackagesProgressBar {
    bar: ProgressBar,
}

impl DownloadingPackagesProgressBar {
    fn new(terminal: &Terminal, to_download: u64) -> Self {
        if terminal.verbosity().is_quiet() {
            return Self {
                bar: ProgressBar::hidden(),
            };
        }
        // `Term` is an `Arc` around inner state, so it's not that expensive to clone.
        let draw_target =
            ProgressDrawTarget::term(terminal.term().clone(), DEFAULT_REFRESH_RATE_HZ);
        let bar = ProgressBar::with_draw_target(Some(to_download), draw_target);
        // Based on https://github.com/console-rs/indicatif/tree/main/examples
        bar.set_style(Self::make_default_style(terminal));
        bar.set_prefix("Downloading");
        Self { bar }
    }

    fn make_default_style(terminal: &Terminal) -> ProgressStyle {
        // https://docs.rs/indicatif/latest/indicatif/style/struct.ProgressStyle.html#method.with_template
        // https://docs.rs/indicatif/latest/indicatif/index.html#templates
        ProgressStyle::with_template(if terminal.term().size().1 > 80 {
            "{prefix:>12.cyan.bold} [{bar:57}] {pos}/{len} {wide_msg}"
        } else {
            "{prefix:>12.cyan.bold} [{bar:57}] {pos}/{len}"
        })
        .expect("We set this statically, it should never fail")
        .progress_chars(PrefixChars::Pacman.as_indicatif_progress_chars())
    }
}

impl Drop for DownloadingPackagesProgressBar {
    fn drop(&mut self) {
        self.bar.finish_and_clear()
    }
}
