from enum import Enum, auto

from quackpack.util.types.pkgid import Identifier


class NoFlag(Enum):
    NoFlag = auto()


type FeatureId = Identifier
type FlagType = FeatureId | NoFlag
