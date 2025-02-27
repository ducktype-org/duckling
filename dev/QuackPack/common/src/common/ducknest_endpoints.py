from common import Package


class DucknestEndpoints:
    @classmethod
    def all_metadata(cls, package: Package) -> str:
        return f"json/{package.name}"

    @classmethod
    def metadata(cls, package: Package) -> str:
        return f"json/{package.name}/{package.version}"

    @classmethod
    def blob(cls, package: Package) -> str:
        return f"static/{package.name}/{package.version}"
