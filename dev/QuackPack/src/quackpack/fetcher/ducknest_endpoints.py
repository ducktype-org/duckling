from quackpack.fetcher.api_types import Package


class DucknestEndpoints:
    @classmethod
    def package(cls, package: Package) -> str:
        return f"packages/{package.name}"

    @classmethod
    def package_version(cls, package: Package) -> str:
        return f"packages/{package.name}/{package.version}"

    @classmethod
    def blob(cls, package: Package) -> str:
        return f"packages/{package.name}/{package.version}/download"
