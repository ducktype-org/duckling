import sys
from collections.abc import (
    ItemsView,
    Iterable,
    Iterator,
    KeysView,
    MutableMapping,
    MutableSequence,
    ValuesView,
)
from typing import Self, SupportsIndex, cast, overload, override

from pydantic import RootModel

# FIXME: Ignores are because RootModel -> BaseModel has weird iterator.


class PydanticMutableDict[K, V](RootModel[dict[K, V]], MutableMapping[K, V]):
    root: dict[K, V] = {}

    @override
    def __setitem__(self, key: K, value: V) -> None:
        self.root[key] = value

    @override
    def __getitem__(self, key: K) -> V:
        return self.root[key]

    @override
    def __delitem__(self, key: K) -> None:
        del self.root[key]

    @override
    def __iter__(self) -> Iterator[K]:  # pyright: ignore[reportIncompatibleMethodOverride]
        return iter(self.root)

    @override
    def __len__(self) -> int:
        return len(self.root)

    @override
    def __contains__(self, o: object) -> bool:
        return o in self.root

    @override
    def keys(self) -> KeysView[K]:
        return self.root.keys()

    @override
    def values(self) -> ValuesView[V]:
        return self.root.values()

    @override
    def items(self) -> ItemsView[K, V]:
        return self.root.items()

    @overload
    def get(self, key: K) -> V | None: ...

    @overload
    def get[T](self, key: K, default: V | T) -> V | T: ...

    @override
    def get[T](self, key: K, default: V | T | None = None) -> V | T | None:
        return self.root.get(key, default)

    @override
    def __eq__(self, other: object) -> bool:
        return self.root == other

    @override
    def __ne__(self, other: object) -> bool:
        return self.root != other

    @overload
    def pop(self, key: K) -> V: ...

    @overload
    def pop(self, key: K, default: V) -> V: ...

    @overload
    def pop[T](self, key: K, default: T) -> V | T: ...

    __marker = object()

    # https://github.com/python/cpython/blob/b865871486987e7622a2059981cc8d708f9b04b0/Lib/_collections_abc.py#L929
    @override
    def pop[T](self, key: K, default: V | T = __marker) -> V | T:
        try:
            value = self[key]
        except KeyError:
            if default is self.__marker:
                raise
            return default
        else:
            del self[key]
            return value

    @override
    def popitem(self) -> tuple[K, V]:
        return self.root.popitem()

    @override
    def clear(self) -> None:
        self.root.clear()


class PydanticMutableList[T](RootModel[list[T]], MutableSequence[T]):
    root: list[T] = []

    @override
    def __iter__(self) -> Iterator[T]:  # pyright: ignore[reportIncompatibleMethodOverride]
        return iter(self.root)

    @overload
    def __getitem__(self, index: SupportsIndex) -> T: ...

    @overload
    def __getitem__(self, index: slice) -> MutableSequence[T]: ...

    @override
    def __getitem__(self, index: slice | SupportsIndex) -> T | MutableSequence[T]:
        return self.root[index]

    @overload
    def __setitem__(self, index: SupportsIndex, item: T) -> None: ...

    @overload
    def __setitem__(self, index: slice, item: Iterable[T]) -> None: ...

    @override
    def __setitem__(self, index: slice | SupportsIndex, item: Iterable[T] | T) -> None:
        if isinstance(index, slice) and not isinstance(item, Iterable):
            raise TypeError("Can only assign Iterable to a slice")
        if isinstance(index, SupportsIndex) and isinstance(item, Iterable):
            raise TypeError("Cannot assign Iterable to an index")
        if isinstance(index, slice):
            self.root[index] = list(*item)
        else:
            assert not isinstance(item, Iterable), "checked above"
            self.root[index] = item

    @overload
    def __delitem__(self, index: SupportsIndex) -> None: ...

    @overload
    def __delitem__(self, index: slice) -> None: ...

    @override
    def __delitem__(self, index: slice | SupportsIndex) -> None:
        del self.root[index]

    @override
    def __len__(self) -> int:
        return len(self.root)

    @override
    def __contains__(self, o: object) -> bool:
        return o in self.root

    @override
    def __reversed__(self) -> Iterator[T]:
        return reversed(self.root)

    @override
    def index(self, value: T, start: SupportsIndex = 0, stop: SupportsIndex = sys.maxsize) -> int:
        return self.root.index(value, start, stop)

    @override
    def count(self, value: T) -> int:
        return self.root.count(value)

    @override
    def append(self, value: T) -> None:
        self.root.append(value)

    @override
    def clear(self) -> None:
        self.root.clear()

    @override
    def reverse(self) -> None:
        self.root.reverse()

    @override
    def extend(self, values: Iterable[T]) -> None:
        self.root.extend(values)

    @override
    def pop(self, index: SupportsIndex = -1) -> T:
        return self.root.pop(index)

    @override
    def remove(self, value: T) -> None:
        self.root.remove(value)

    @override
    def insert(self, index: SupportsIndex, value: T) -> None:
        self.root.insert(index, value)

    @override
    def __iadd__(self, values: Iterable[T]) -> Self:
        return cast(Self, self.root.__iadd__(values))
