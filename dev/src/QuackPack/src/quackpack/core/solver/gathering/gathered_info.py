from dataclasses import dataclass

from quackpack.core.solver.types.flag_type import FeatureId
from quackpack.core.solver.types.packages_by_id import PackagesById
from quackpack.core.solver.types.resolved_package import ResolvedPackage
from quackpack.core.solver.types.unresolved_id import IdResolvents
from quackpack.core.types.manifest.summary import Summary


@dataclass
class GatheredInfo:
    summaries: dict[ResolvedPackage, Summary]
    possible_features: dict[ResolvedPackage, set[FeatureId]]
    packages_by_id: PackagesById
    id_resolvents: IdResolvents
