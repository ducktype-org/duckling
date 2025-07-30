from dataclasses import dataclass

from quackpack.manifest.summary import Summary
from quackpack.solver.types.flag_type import FeatureId
from quackpack.solver.types.packages_by_id import PackagesById
from quackpack.solver.types.resolved_package import ResolvedPackage
from quackpack.solver.types.unresolved_id import IdResolvents


@dataclass
class GatheredInfo:
    summaries: dict[ResolvedPackage, Summary]
    possible_features: dict[ResolvedPackage, set[FeatureId]]
    packages_by_id: PackagesById
    id_resolvents: IdResolvents
