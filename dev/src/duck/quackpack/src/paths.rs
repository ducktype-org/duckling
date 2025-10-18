use rustvil::config_files::xdg::{self, MacOSBehaviour};
use rustvil::os::env::Env;
use std::path::PathBuf;

pub const MANIFEST_FILENAME: &str = "quackconfig.yml";
pub const FREEZEFILE_NAME: &str = "quackfreeze.json";
pub const VENV_CONFIG_FILENAME: &str = "venvconfig.toml";
pub const LOCAL_STORAGE_DIR_NAME: &str = ".storage";

const QP_CONFIG_RELATIVE_TO_CONFIG_DIR: &str = "qp";
const QP_CACHE_RELATIVE_TO_CACHE_DIR: &str = "qp";
const QP_DATA_RELATIVE_TO_DATA_DIR: &str = "qp";

const QP_CONFIG_ENV: &str = "QP_CONFIG";
const QP_CACHE_ENV: &str = "QP_CACHE";
const QP_STORAGE_ENV: &str = "QP_STORAGE";
const QP_GLOBAL_STORAGE_ENV: &str = "QP_GLOBAL_ENV";

fn config_dir(env: &Env, behaviour: MacOSBehaviour) -> Option<PathBuf> {
    env.get_os(QP_CONFIG_ENV).map(PathBuf::from).or_else(|| {
        xdg::config(env, behaviour).map(|mut config| {
            config.push(QP_CONFIG_RELATIVE_TO_CONFIG_DIR);
            config
        })
    })
}

fn cache_dir(env: &Env, behaviour: MacOSBehaviour) -> Option<PathBuf> {
    env.get_os(QP_CACHE_ENV).map(PathBuf::from).or_else(|| {
        xdg::cache(env, behaviour).map(|mut config| {
            config.push(QP_CACHE_RELATIVE_TO_CACHE_DIR);
            config
        })
    })
}

fn default_storage_dir(env: &Env, behaviour: MacOSBehaviour) -> Option<PathBuf> {
    env.get_os(QP_STORAGE_ENV).map(PathBuf::from).or_else(|| {
        xdg::data(env, behaviour).map(|mut config| {
            config.push(QP_DATA_RELATIVE_TO_DATA_DIR);
            config
        })
    })
}

fn default_global_venv_dir(env: &Env, behaviour: MacOSBehaviour) -> Option<PathBuf> {
    env.get_os(QP_GLOBAL_STORAGE_ENV)
        .map(PathBuf::from)
        .or_else(|| {
            xdg::data(env, behaviour).map(|mut config| {
                config.push(QP_DATA_RELATIVE_TO_DATA_DIR);
                config
            })
        })
}

pub fn download_dir(env: &Env) -> Option<PathBuf> {
    cache_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("downloads");
        buf
    })
}

pub fn artifacts_dir(env: &Env) -> Option<PathBuf> {
    cache_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("artifacts");
        buf
    })
}

pub fn metadata_db(env: &Env) -> Option<PathBuf> {
    cache_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("metadata_db.sqlite");
        buf
    })
}

pub fn fetcher_lockfile(env: &Env) -> Option<PathBuf> {
    cache_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("fetcher.lock");
        buf
    })
}

pub fn storage_dir(env: &Env) -> Option<PathBuf> {
    default_storage_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("storage");
        buf
    })
}

pub fn global_venv_dir(env: &Env) -> Option<PathBuf> {
    default_global_venv_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("global_venv");
        buf
    })
}

pub fn config_file(env: &Env) -> Option<PathBuf> {
    config_dir(env, MacOSBehaviour::LinuxFallback).map(|mut buf| {
        buf.push("config.toml");
        buf
    })
}
