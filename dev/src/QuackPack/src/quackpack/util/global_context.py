from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from quackpack.core.constants import (
    DEFAULT_REGISTRY_URL,
    DEFAULT_STORAGE_TMP_LIFETIME_SECONDS,
    DEFAULT_TYPO_TOLERANCE_DISTANCE,
    SHOULD_BUILD_FROM_SOURCE,
    SHOULD_FIX_TYPOS,
    artifacts_dir,
    config_filepath,
    download_dir,
    fetcher_lockfile,
    global_venv_dir,
    metadata_db,
    storage_dir,
)
from quackpack.core.types.manifest.schemas.manifest import CompilerOptionsSchema
from quackpack.driver.cli.console import Console
from quackpack.util.env import Env
from quackpack.util.logger import get_logger
from quackpack.util.toml_config import TOMLConfig

__all__ = ["GlobalContext"]


logger = get_logger(__name__)


@dataclass(frozen=False, kw_only=True)
class GlobalContext:
    """
    Global information used everywhere throughout Quack Pack implementation.
    """

    user_config: TOMLConfig
    """
    User configuration.
    """

    console: Console
    """
    `quackpack.util.console.Console` used for printing to user.
    """

    error_console: Console
    """
    `quackpack.util.console.Console` set up for `sys.stderr`.
    """

    env: Env

    @classmethod
    def default(cls) -> GlobalContext:
        return cls(
            console=Console(),
            error_console=Console(stderr=True),
            user_config=TOMLConfig.default(),
            env=Env.default(),
        )

    @classmethod
    def create_with_consoles_and_env(
        cls, *, console: Console, error_console: Console, env: Env
    ) -> GlobalContext:
        config_path = config_filepath(env)
        user_config = TOMLConfig.create_from_filepath(config_path)

        return cls(
            console=console,
            error_console=error_console,
            user_config=user_config,
            env=env,
        )

    def download_dir(self) -> Path:
        return (
            self.user_config.get_path("cache.download_dir")
            or download_dir(self.env).expanduser()
        )

    def ensure_download_dir(self) -> Path:
        ret = self.download_dir()
        self.user_config.ensure_dir(ret)
        return ret

    def artifacts_dir(self) -> Path:
        return (
            self.user_config.get_path("cache.artifacts_dir")
            or artifacts_dir(self.env).expanduser()
        )

    def ensure_artifacts_dir(self) -> Path:
        ret = self.artifacts_dir()
        self.user_config.ensure_dir(ret)
        return ret

    def metadata_db(self) -> Path:
        return (
            self.user_config.get_path("cache.metadata_db_path")
            or metadata_db(self.env).expanduser()
        )

    def ensure_metadata_db(self) -> Path:
        ret = self.metadata_db()
        self.user_config.ensure_path(ret)
        return ret

    def fetcher_lockfile(self) -> Path:
        return (
            self.user_config.get_path("cache.fetcher_lockfile")
            or fetcher_lockfile(self.env).expanduser()
        )

    def ensure_fetcher_lockfile(self) -> Path:
        ret = self.fetcher_lockfile()
        self.user_config.ensure_path(ret)
        return ret

    def targets(self) -> CompilerOptionsSchema:
        table = self.user_config.get_table("build.targets") or {}
        return CompilerOptionsSchema.model_validate(table)

    def profiles(self) -> CompilerOptionsSchema:
        table = self.user_config.get_table("build.profiles") or {}
        return CompilerOptionsSchema.model_validate(table)

    def registry_url(self) -> str:
        return self.user_config.get_str("registry.url") or DEFAULT_REGISTRY_URL

    def are_typos_enabled(self) -> bool:
        return self.user_config.get_bool("security.typos.enabled") or SHOULD_FIX_TYPOS

    def typos_distance(self) -> int:
        return (
            self.user_config.get_int("security.typos.max_distance")
            or DEFAULT_TYPO_TOLERANCE_DISTANCE
        )

    def storage_dir(self) -> Path:
        return (
            self.user_config.get_path("storage.dir")
            or storage_dir(self.env).expanduser()
        )

    def ensure_storage_dir(self) -> Path:
        ret = self.storage_dir()
        self.user_config.ensure_dir(ret)
        return ret

    def storage_tmp_lifetime(self) -> int:
        return (
            self.user_config.get_int("storage.temporary_lifetime")
            or DEFAULT_STORAGE_TMP_LIFETIME_SECONDS
        )

    def global_venv_dir(self) -> Path:
        return (
            self.user_config.get_path("global_venv")
            or global_venv_dir(self.env).expanduser()
        )

    def ensure_global_venv_dir(self) -> Path:
        ret = self.global_venv_dir()
        self.user_config.ensure_dir(ret)
        return ret

    def get_alias(self, name: str) -> str | None:
        return self.user_config.get_str(f"aliases.{name}")

    def aliases(self) -> dict[str, Any]:
        return self.user_config.get_table("aliases") or {}

    def builds_from_source(self) -> bool:
        return (
            self.user_config.get_bool("packaging.build_from_source")
            or SHOULD_BUILD_FROM_SOURCE
        )
