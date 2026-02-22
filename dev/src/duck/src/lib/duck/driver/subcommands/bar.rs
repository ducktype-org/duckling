//! WIP: test command for checking progress bars

use std::hash::Hasher;
use std::hash::{DefaultHasher, Hash};
use std::time::Duration;

use crate::StrId;
use crate::{
    DuckCtx, QuackResult, quackpack::util::progress_bar::DownloadingPackagesProgressBarManager,
};
use clap::{ArgMatches, Command};

use crate::duck::driver::cli_ext::subcommand;
use async_scoped::TokioScope;
use tokio::runtime::Builder;

pub fn get_parser() -> Command {
    subcommand("bar").about("[WIP] Test progress bar")
}

pub fn execute(ctx: &DuckCtx, _matches: &ArgMatches) -> QuackResult<()> {
    Builder::new_multi_thread()
        .enable_all()
        .build()
        .unwrap()
        .block_on(async move {
            let pkgs = vec![
                StrId::from("foo"),
                StrId::from("bar"),
                StrId::from("baz"),
                StrId::from("duck"),
                StrId::from("compiler"),
            ];
            let bar = DownloadingPackagesProgressBarManager::new(ctx.console(), pkgs.clone());

            TokioScope::scope_and_block(|spawner| {
                for pkg in pkgs.iter() {
                    let download = || async {
                        let mut hasher = DefaultHasher::new();
                        pkg.hash(&mut hasher);
                        let hash = hasher.finish();

                        let time_to_sleep = hash % 2 + 1;
                        tokio::time::sleep(Duration::from_secs(time_to_sleep)).await;
                        bar.start_download_of(*pkg).await;

                        let time_to_sleep = hash % 5 + 1 + ((*pkg == "foo") as u64);
                        tokio::time::sleep(Duration::from_secs(time_to_sleep)).await;
                        bar.finish_download_of(*pkg).await;
                    };
                    spawner.spawn(download());
                }
            });
        });
    Ok(())
}
