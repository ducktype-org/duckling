use std::sync::Barrier;
use std::sync::atomic::{AtomicUsize, Ordering};

use super::{registry_url_hash, setup_mock_storage};
use crate::StrId;
use crate::quackpack::core::storage::venv_id::ToVenvId;
use crate::quackpack::core::storage::{self};

#[test]
/// Only one thread should be able to delete a given venv.
/// The first thread should successfully remove this venv, while other should:
/// 1. either perform no-op (removing non existent venv)
/// 2. or fail with the message below, if the first thread is in process of deleting the venv.
///
/// We should have at most `threads - 1` failures (but we may not have exactly that many, because
/// of 1.)
fn concurrent_delete() {
    let (ctx, _home, storage_root) = setup_mock_storage();
    let thread_count = 4;
    let barrier = Barrier::new(thread_count);
    let lock_failures = AtomicUsize::default();

    std::thread::scope(|s| {
        for _ in 0..thread_count {
            s.spawn(|| {
                barrier.wait();
                let result = storage::ops::delete_venv(
                    &ctx,
                    ctx.default_storage_root().not_locked_path(),
                    StrId::new("venv1"),
                );
                if let Err(e) = result {
                    lock_failures.fetch_add(1, Ordering::SeqCst);
                    assert_eq!(
                        e.to_string(),
                        format!(
                            "another synchronization operation is ongoing in venv `venv1`
failed to acquire an exclusive lock on `{}`
operation would block",
                            storage_root
                                .join("locks")
                                .join("venv_sync")
                                .join("venv1")
                                .display()
                        )
                    );
                }
            });
        }
    });
    // We can't do anything better, because we can finish delete before any other thread has
    // CPU time.
    assert!(lock_failures.load(Ordering::SeqCst) < thread_count);
}

#[test]
/// This tests two things:
/// 1. clean is a blocking operation,
/// 2. cleaning an empty venv has empty result
fn concurrent_clean() {
    let (ctx, _home, root) = setup_mock_storage();
    let thread_count = 4;
    let barrier = Barrier::new(thread_count);
    let empty_cleans = AtomicUsize::default();

    let mut expected_packages = [
        root.join("pkg")
            .join(format!("registry-{}-foo-1.0.0", registry_url_hash())),
        root.join("pkg")
            .join(format!("registry-{}-bar-1.0.0", registry_url_hash())),
    ];
    expected_packages.sort();
    std::thread::scope(|s| {
        for _ in 0..thread_count {
            s.spawn(|| {
                barrier.wait();
                let mut result =
                    storage::ops::clean_storage(&ctx, ctx.default_storage_root().not_locked_path())
                        .unwrap();
                if !result.removed_packages.is_empty() {
                    result.removed_packages.sort();
                    assert_eq!(result.removed_venvs, ["root3".to_venv_id()]);
                    assert_eq!(result.removed_packages, expected_packages);
                } else {
                    assert!(result.removed_venvs.is_empty());
                    assert!(result.removed_packages.is_empty());
                    empty_cleans.fetch_add(1, Ordering::SeqCst);
                }
            });
        }
    });
    assert_eq!(empty_cleans.load(Ordering::SeqCst), thread_count - 1);
}

#[test]
/// Concurrent deletes on different venvs should not block and both should succeed.
fn concurrent_different_deletes() {
    let (ctx, _home, _root) = setup_mock_storage();
    let thread_count = 2;
    let barrier = Barrier::new(thread_count);

    std::thread::scope(|s| {
        for _ in 0..thread_count {
            s.spawn(|| {
                let is_leader = barrier.wait().is_leader();

                let venv = if is_leader { "venv1" } else { "venv2" };

                storage::ops::delete_venv(&ctx, ctx.default_storage_root().not_locked_path(), venv)
                    .unwrap();
            });
        }
    });
}
