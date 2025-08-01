"""
Module for representing HTTP headers.
----
Classes:
- `HTTPHeaders`: A dictionary that maintains Http-Header-Case for all keys and supports multiple values per key.
- `ResponseStartLine`: Represents the start line of an HTTP response.
----
References:
  [1] https://github.com/tornadoweb/tornado/blob/master/tornado/httputil.py
"""

from __future__ import annotations

import re
from collections.abc import Iterable, Iterator, MutableMapping
from functools import lru_cache
from typing import Any, Final, NamedTuple, override

from quackpack.fetcher.util.http_errors import HTTPInputError

_CRLF_RE: Final[re.Pattern[str]] = re.compile(r"\r?\n")
_HTTP_WHITESPACE: Final[str] = " \t"


@lru_cache(1000)
def _normalize_header(name: str) -> str:
    """
    Normalize a header name to Http-Header-Case.

    :param str name: The header name to normalize.
    :return: The normalized header name.
    :rtype: str
    """

    return "-".join(w.capitalize() for w in name.split("-"))


class HTTPHeaders(MutableMapping[str, str]):
    """
    A dictionary that maintains Http-Header-Case for all keys.
    Supports multiple values per key via ``add()`` and ``get_list()``.
    """

    def __init__(self, *args: Any, **kwargs: str) -> None:
        self._dict: dict[str, str] = {}
        self._as_list: dict[str, list[str]] = {}
        self._last_key = None
        if len(args) == 1 and len(kwargs) == 0 and isinstance(args[0], HTTPHeaders):
            # NOTE: Original comment.
            # Copy constructor
            for k, v in args[0].get_all():
                self.add(k, v)
        else:
            # NOTE: Original comment.
            # Dict-style initialization
            self.update(*args, **kwargs)

    def add(self, name: str, value: str) -> None:
        """
        Add a new value for the given key.

        :param str name: The header name.
        :param str value: The header value.
        """

        norm_name = _normalize_header(name)
        self._last_key = norm_name
        if norm_name in self:
            self._dict[norm_name] = f"{self[norm_name]},{value}"
            self._as_list[norm_name].append(value)
        else:
            self[norm_name] = value

    def get_list(self, name: str) -> list[str]:
        """
        Get all values for the given header as a list.

        :param str name: The header name.
        :return: A list of values for the header.
        :rtype: list[str]
        """

        norm_name = _normalize_header(name)
        return self._as_list.get(norm_name, [])

    def get_all(self) -> Iterable[tuple[str, str]]:
        """
        Get an iterable of all (name, value) pairs.

        :return: An iterable of (name, value) pairs.
        :rtype: Iterable[tuple[str, str]]
        """

        for name, values in self._as_list.items():
            for value in values:
                yield name, value

    def parse_line(self, line: str) -> None:
        """
        Update the dictionary with a single header line.

        :param str line: The header line to parse.
        :raises quackpack.fetcher.util.HTTPInputError: If the header line is malformed.
        """

        if line[0].isspace():
            # NOTE: Original comment.
            # continuation of a multi-line header
            if self._last_key is None:
                raise HTTPInputError("first header line cannot start with whitespace")
            new_part = " " + line.lstrip(_HTTP_WHITESPACE)
            self._as_list[self._last_key][-1] += new_part
            self._dict[self._last_key] += new_part
        else:
            try:
                name, value = line.split(":", 1)
            except ValueError as e:
                raise HTTPInputError("no colon in header line") from e
            self.add(name, value.strip(_HTTP_WHITESPACE))

    @classmethod
    def parse(cls, headers: str) -> HTTPHeaders:
        """
        Parse HTTP header text into an ``HTTPHeaders`` object.

        :param str headers: The header text to parse.
        :return: The parsed headers.
        :rtype: quackpack.fetcher.util.HTTPHeaders
        """

        h = cls()
        for line in _CRLF_RE.split(headers):
            if line:
                h.parse_line(line)
        return h

    @override
    def __setitem__(self, name: str, value: str) -> None:
        norm_name = _normalize_header(name)
        self._dict[norm_name] = value
        self._as_list[norm_name] = [value]

    @override
    def __getitem__(self, name: str) -> str:
        return self._dict[_normalize_header(name)]

    @override
    def __delitem__(self, name: str) -> None:
        norm_name = _normalize_header(name)
        del self._dict[norm_name]
        del self._as_list[norm_name]

    @override
    def __len__(self) -> int:
        return len(self._dict)

    @override
    def __iter__(self) -> Iterator[str]:
        return iter(self._dict)

    def copy(self) -> HTTPHeaders:
        """
        Create a shallow copy of the ``HTTPHeaders`` object.

        :return: A shallow copy of the object.
        :rtype: quackpack.fetcher.util.HTTPHeaders
        """

        # NOTE: Original comment.
        # defined in dict but not in MutableMapping.
        return HTTPHeaders(self)

    # NOTE: Original comment.
    # Use our overridden copy method for the copy.copy module.
    # This makes shallow copies one level deeper, but preserves
    # the appearance that HTTPHeaders is a single container.
    __copy__ = copy

    @override
    def __str__(self) -> str:
        lines: list[str] = []
        for name, value in self.get_all():
            lines.append(f"{name}: {value}\n")
        return "".join(lines)

    __unicode__ = __str__


class ResponseStartLine(NamedTuple):
    """
    Represents the start line of an HTTP response.

    :param str version: HTTP version.
    :param int code: HTTP status code.
    :param str reason: HTTP reason phrase.
    """

    version: str
    code: int
    reason: str


def parse_http1_response_start_line(line: str) -> ResponseStartLine:
    """
    Parse an HTTP 1.x response start line into a ``ResponseStartLine`` tuple.

    :param str line: The HTTP response start line as a string.
    :return: A named tuple containing ``version``, ``code``, and ``reason``.
    :rtype: quackpack.fetcher.util.ResponseStartLine
    :raises quackpack.fetcher.util.HTTPInputError: If the input line is not a valid HTTP 1.x response start line.
    """

    match = re.match(r"(HTTP/1.[0-9]) ([0-9]+) ([^\r]*)", line)
    if not match:
        raise HTTPInputError("Error parsing response start line")
    return ResponseStartLine(match.group(1), int(match.group(2)), match.group(3))
