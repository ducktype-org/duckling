use rustvil::fs::{MkdirOptions, PathExt};
use tempfile::tempdir;

use crate::{DuckCtx, QpCtx, quackpack::core::PackageLoader, util_common::error::ErrorExt};

const BASIC_MANIFEST: &str = r"
metadata:
  name: foo
  version: 0.1
";

#[test]
fn no_package_from_directory() {
    let tmp_file = tempdir().unwrap();
    let ctx = DuckCtx::default();
    let err = PackageLoader::find_from_directory(tmp_file.path(), &QpCtx::new(&ctx), false.into())
        .unwrap_err();
    assert_eq!(
        err.all_errors_to_vec(),
        [format!(
            "no manifest has been found from the `{}` to the `/`",
            tmp_file.path().resolve().unwrap().display()
        )]
    );
}

#[test]
fn not_a_dir() {
    let tmp_file = tempdir().unwrap();
    let file = tmp_file.path().join("x");
    let ctx = DuckCtx::default();
    let err =
        PackageLoader::find_from_directory(&file, &QpCtx::new(&ctx), false.into()).unwrap_err();
    assert_eq!(
        err.all_errors_to_vec(),
        [format!(
            "the path `{}` is not a directory",
            file.resolve().unwrap().display()
        )]
    );
}

#[test]
fn founds_from_directory() {
    let tmp_file = tempdir().unwrap();
    let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
    file.touch().unwrap();
    file.write(BASIC_MANIFEST).unwrap();
    let ctx = DuckCtx::default();
    let qpctx = QpCtx::new(&ctx);
    let package =
        PackageLoader::find_from_directory(tmp_file.path(), &qpctx, false.into()).unwrap();
    assert_eq!(
        package.package().root_directory().resolve().unwrap(),
        tmp_file.path().resolve().unwrap()
    );
}

#[test]
fn founds_at_parent() {
    let tmp_file = tempdir().unwrap();
    let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
    file.touch().unwrap();
    file.write(BASIC_MANIFEST).unwrap();
    let child = tmp_file.path().join("foo");
    child.mkdir(MkdirOptions::WithoutParents).unwrap();
    assert!(child.is_dir());
    let ctx = DuckCtx::default();
    let qpctx = QpCtx::new(&ctx);
    let package = PackageLoader::find_from_directory(&child, &qpctx, false.into()).unwrap();
    assert_eq!(
        package.package().root_directory().resolve().unwrap(),
        tmp_file.path().resolve().unwrap()
    );
}

#[test]
fn founds_at_exact_directory() {
    let tmp_file = tempdir().unwrap();
    let file = tmp_file.path().join(PackageLoader::MANIFEST_NAME);
    file.touch().unwrap();
    file.write(BASIC_MANIFEST).unwrap();
    let ctx = DuckCtx::default();
    let qpctx = QpCtx::new(&ctx);
    let package = PackageLoader::find_at_exact_directory(tmp_file.path(), &qpctx).unwrap();
    assert_eq!(
        package.package().root_directory().resolve().unwrap(),
        tmp_file.path().resolve().unwrap()
    );
}

#[test]
fn founds_at_exact_directory_noadir() {
    let tmp_file = tempdir().unwrap();
    let file = tmp_file.path().join("xd");
    assert!(!file.exists());
    let ctx = DuckCtx::default();
    let qpctx = QpCtx::new(&ctx);
    let err = PackageLoader::find_at_exact_directory(&file, &qpctx).unwrap_err();
    assert_eq!(
        err.all_errors_to_vec(),
        [format!("the path `{}` is not a directory", file.display())]
    );

    file.touch().unwrap();
    assert!(!file.is_dir());

    let err = PackageLoader::find_at_exact_directory(&file, &qpctx).unwrap_err();
    assert_eq!(
        err.all_errors_to_vec(),
        [format!("the path `{}` is not a directory", file.display())]
    );
}
