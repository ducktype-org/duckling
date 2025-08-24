"""
Module containing endpoints for Ducknest instance interaction.
"""

import urllib.parse

from quackpack.core.fetcher.api_types import Package, PackageName


class DucknestEndpoints:
    @classmethod
    def get_package_multi(cls, package_id: PackageName) -> str:
        return f"packages/{package_id}"

    @classmethod
    def get_package_single(cls, package: Package) -> str:
        return f"packages/{package.id}/{package.version}"

    @classmethod
    def get_package_blob(cls, package: Package) -> str:
        return f"packages/{package.id}/{package.version}/download"

    @classmethod
    def get_search(cls, query: str) -> str:
        encoded_query = urllib.parse.quote(query)

        return f"packages?q={encoded_query}"

    @classmethod
    def post_package(cls) -> str:
        return "packages"

    @classmethod
    def put_package_blob(cls, package: Package) -> str:
        return f"packages/{package.id}/{package.version}"
