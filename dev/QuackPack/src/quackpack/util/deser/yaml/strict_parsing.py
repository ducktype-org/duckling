import contextlib
from enum import Enum
from io import StringIO
from math import log10
from pathlib import Path
from typing import Any

import yaml
from pydantic import BaseModel, ValidationError

from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


def get_file_fragment(path: Path, start: tuple[int, int], end: tuple[int, int]) -> str:
    """
    Returns the invalid fragment of the file, marked red, with two lines of context (one before and one after).
    Adds line numbers before each line.

    Args:
        path (Path): path to the problematic file.
        start (tuple[int, int]): pair (line, column), describing where the problematic fragment begins.
        end (tuple[int, int]): pair (line, column), describing where the problematic fragment ends.

    Returns:
        str: Fragment of the file with numbered lines and red marking.
    """
    start_line, start_column = start
    end_line, end_column = end

    def digits_in_int(n: int) -> int:
        if n <= 0:
            raise ValueError("Non positive line number?!")
        return int(log10(n)) + 1

    with open(path) as f:
        raw_str = f.read()
        lines = raw_str.splitlines()
    max_line_number = min(end_line + 1, len(lines))
    max_line_number_indentation_length = digits_in_int(max_line_number)

    buffer = StringIO()
    for i, line in enumerate(lines):
        if i + 1 < start_line or i - 1 > end_line:
            continue
        line += " "
        line_number_indentation_length = max_line_number_indentation_length - digits_in_int(i + 1)
        buffer.write("[blue]")
        for _ in range(line_number_indentation_length):
            buffer.write(" ")
        buffer.write(str(i + 1))
        buffer.write(".[/blue]")
        if i + 1 == start_line or i - 1 == end_line:
            # This is a context line, print it normally
            buffer.write(line)
        elif start_line == end_line:
            # This is the only line of the error
            buffer.write(line[:start_column])
            buffer.write("[white on red]")
            buffer.write(line[start_column:end_column])
            buffer.write("[/white on red]")
            buffer.write(line[end_column:])
        elif i == start_line:
            buffer.write(line[:start_column])
            buffer.write("[white on red]")
            buffer.write(line[start_column:])
            buffer.write("[/white on red]")
        elif i == end_line:
            buffer.write("[white on red]")
            buffer.write(line[:end_column])
            buffer.write("[/white on red]")
            buffer.write(line[end_column:])
        else:
            # We are after the start_line but before the end_line.
            # Thus, the line is between the [white on red] and [/white on red] tags.
            buffer.write("[white on red]")
            buffer.write(line)
            buffer.write("[/white on red]")
        buffer.write("\n")
    return buffer.getvalue()


def verbose_error_message(path: Path, msg: str, file_fragment: str | None = None) -> str:
    """
    Creates a pretty error message, compliant with rich.console.

    Args:
        path (Path): Path to the file which caused problems.
        msg (str): Error message.
        file_fragment (str | None, optional): Additional file fragment to display. Defaults to None.

    Returns:
        str: A pretty and verbose error message
    """
    display_path = path
    with contextlib.suppress(ValueError):
        display_path = path.relative_to(Path.cwd())
    if file_fragment is not None:
        formatted_error = f"[bold red]ERROR![/bold red] in the file: [green]{display_path}[/green]\n\n{file_fragment}\n{msg}"
    else:
        formatted_error = f"[bold red]ERROR![/bold red] While trying to parse the file: [green]{display_path}[/green]\n\n{msg}"
    return formatted_error


class ExpectedToken(Enum):
    KEY = 0
    VALUE = 1
    ANY = 2


class YamlStructureType(Enum):
    MARKED_LIST = 0
    IMPLICIT_LIST = 1
    DICTIONARY = 2
    IMPLICIT_DICTIONARY = 3


class LocationInitializer(Enum):
    NEW_KEY = 0
    NEW_FLOW_OR_UNMARKED_LIST = 1
    NEW_MARKED_BLOCK_LIST = 2


class LocationSpecifier:
    value: int | str

    def __init__(self, option: LocationInitializer):
        match option:
            case LocationInitializer.NEW_KEY:
                self.value = ""
            case LocationInitializer.NEW_FLOW_OR_UNMARKED_LIST:
                self.value = 0
            case LocationInitializer.NEW_MARKED_BLOCK_LIST:
                self.value = -1

    @classmethod
    def create_with_key(cls, key: str):
        result = cls(LocationInitializer.NEW_KEY)
        result.set_key(key)
        return result

    def set_key(self, new_key: str):
        assert isinstance(self.value, str)
        self.value = new_key

    def increment_index(self):
        assert isinstance(self.value, int)
        self.value += 1

    def downgrade(self) -> int | str:
        return self.value


def deserialize_and_tokenise(path: Path) -> tuple[Any, list[yaml.Token]]:
    """
    Helper function for strict_safe_load.
    Deserializes a file and returns its tokenization.
    To circumvent the so-called 'Norway problem', yaml.BaseLoader is used.

    Args:
        path (Path): path to the file

    Raises:
        ConfigFileLoadError: Custom error, signifying deserialization failed.

    Returns:
        tuple[Any, list[yaml.TagToken]]:
            - first element: deserialized object
            - second element: the list of the tokens
    """
    with open(path) as config_file:
        try:
            deserialized_object = yaml.load(config_file, Loader=yaml.BaseLoader)
        except yaml.MarkedYAMLError as exp:
            if exp.problem_mark is not None:
                start_line, start_column, end_line, end_column = (
                    exp.problem_mark.line,
                    exp.problem_mark.column,
                    exp.problem_mark.line,
                    exp.problem_mark.column + 1,
                )
                raise ConfigFileLoadError(
                    verbose_error_message(
                        path,
                        "Failed to deserialize the file",
                        get_file_fragment(path, (start_line, start_column), (end_line, end_column)),
                    )
                ) from None
            else:
                raise ConfigFileLoadError(verbose_error_message(path, str(exp))) from None
        except yaml.YAMLError as exp:
            raise ConfigFileLoadError(verbose_error_message(path, str(exp))) from None

    with open(path) as config_file:
        try:
            tokens: list[yaml.Token] = list(yaml.scan(config_file, Loader=yaml.BaseLoader))  # pyright: ignore[reportUnknownArgumentType,reportUnknownMemberType]
        except yaml.MarkedYAMLError as exp:
            if exp.problem_mark is not None:
                start_line, start_column, end_line, end_column = (
                    exp.problem_mark.line,
                    exp.problem_mark.column,
                    exp.problem_mark.line,
                    exp.problem_mark.column + 1,
                )
                raise ConfigFileLoadError(
                    verbose_error_message(
                        path,
                        get_file_fragment(path, (start_line, start_column), (end_line, end_column)),
                        "Invalid YAML syntax",
                    )
                ) from None
            else:
                raise ConfigFileLoadError(verbose_error_message(path, str(exp))) from None
        except yaml.YAMLError as exp:
            raise ConfigFileLoadError(verbose_error_message(path, str(exp))) from None
    return deserialized_object, tokens


def strict_safe_load(path: Path) -> tuple[dict[tuple[int | str, ...], tuple[yaml.Mark, yaml.Mark]], Any]:
    """
    Helper function for load_and_validate.
    Deserializes a yaml file, returns the deserialized object with a
    mapping between entries of the object and fragments of the yaml file, defying those entries.
    To construct the mapping of entries, a custom parsing of yaml tokens is applied.
    Note, that only lists and dictionaries are supported (so no advanced yaml syntax e.g. references is allowed).

    Args:
        path (Path): path of the file to deserialize

    Raises:
        ConfigFileLoadError: Custom error indicating either deserialization or validation issue

    Returns:
        tuple[dict[tuple[int | str, ...], tuple[yaml.Mark, yaml.Mark]], Any]:
            - first element: mapping between series of keys or indexes, identifying a token from the config file
                and a precise location of such token in the config file.
            - second element: deserialized object
    """
    positions: dict[tuple[int | str, ...], tuple[yaml.Mark, yaml.Mark]] = {}

    locations_stack: list[LocationSpecifier] = []
    structures_stack: list[YamlStructureType] = []
    expected: ExpectedToken = ExpectedToken.ANY

    deserialized_object, tokens = deserialize_and_tokenise(path)
    # Positions of parentheses are also stored, which makes it possible to mark them,
    # was the whole object inside them not to validate.
    parentheses_counter: int = 0
    for i, token in enumerate(tokens):
        match type(token):
            case yaml.BlockMappingStartToken | yaml.FlowMappingStartToken:
                # Beginning of a dictionary
                locations_stack.append(LocationSpecifier(LocationInitializer.NEW_KEY))
                structures_stack.append(YamlStructureType.DICTIONARY)
                expected = ExpectedToken.ANY
            case yaml.FlowSequenceStartToken:
                # Beginning of a list enclosed in [].
                locations_stack.append(LocationSpecifier(LocationInitializer.NEW_FLOW_OR_UNMARKED_LIST))
                structures_stack.append(YamlStructureType.MARKED_LIST)
                expected = ExpectedToken.ANY
            case yaml.BlockSequenceStartToken:
                # Beginning of a not enclosed list.
                locations_stack.append(LocationSpecifier(LocationInitializer.NEW_MARKED_BLOCK_LIST))
                structures_stack.append(YamlStructureType.MARKED_LIST)
                expected = ExpectedToken.ANY
            case yaml.FlowEntryToken:
                # This means ',' so a new sequence element or dict entry is coming.
                # This cannot occur inside an implicit dictionary
                if structures_stack[-1] is not YamlStructureType.DICTIONARY:
                    locations_stack[-1].increment_index()
                expected = ExpectedToken.ANY
            case yaml.BlockEntryToken:
                # This can mean a new not enclosed list or a continuation of the current one
                # Beginning of a not enclosed list can only happen as a value of a key.
                if structures_stack and structures_stack[-1] is not YamlStructureType.DICTIONARY:
                    # Continuation of a list.
                    locations_stack[-1].increment_index()
                else:
                    # Beginning of a new list
                    locations_stack.append(LocationSpecifier(LocationInitializer.NEW_FLOW_OR_UNMARKED_LIST))
                    structures_stack.append(YamlStructureType.IMPLICIT_LIST)
                    # We add the position of the '-' to the dictionary.
                    # Thus, it can be detected as a part of the problem-causing file fragment,
                    # was the validation to fail.
                    precise_location = tuple(
                        location.downgrade()
                        for location in (
                            *locations_stack,
                            LocationSpecifier.create_with_key(f"!{parentheses_counter}"),
                        )
                    )
                    positions[precise_location] = (token.start_mark, token.end_mark)
                    parentheses_counter += 1
                expected = ExpectedToken.ANY
            case yaml.BlockEndToken | yaml.FlowMappingEndToken:
                # Ending of a dictionary or a list.
                locations_stack.pop()
                structures_stack.pop()
                if (
                    structures_stack
                    and structures_stack[-1] is YamlStructureType.IMPLICIT_LIST
                    and not isinstance(tokens[i + 1], yaml.BlockEntryToken)
                    and not isinstance(tokens[i + 1], yaml.FlowEntryToken)
                ):
                    # We have just ended an element of an implicit list (not marked with ...SequenceBeginToken).
                    # Moreover, the next token is not a new element of the list, so the list ends with current token.
                    # Only one level of pops is needed, because implicit lists cannot be directly nested.
                    locations_stack.pop()
                    structures_stack.pop()
                elif structures_stack and structures_stack[-1] is YamlStructureType.IMPLICIT_DICTIONARY:
                    # If we were inside an implicit dictionary, we pop twice
                    locations_stack.pop()
                    structures_stack.pop()
                expected = ExpectedToken.ANY
            case yaml.FlowSequenceEndToken:
                # Ending of an enclosed list.
                locations_stack.pop()
                structures_stack.pop()
                expected = ExpectedToken.ANY
                if structures_stack and structures_stack[-1] is YamlStructureType.IMPLICIT_DICTIONARY:
                    # If we were inside an implicit dictionary, we pop twice.
                    locations_stack.pop()
                    structures_stack.pop()
            case yaml.KeyToken:
                # A key is coming.
                expected = ExpectedToken.KEY
                # If we are inside a marked list, this means a start of an implicit dictionary.
                if locations_stack and structures_stack[-1] is YamlStructureType.MARKED_LIST:
                    structures_stack.append(YamlStructureType.IMPLICIT_DICTIONARY)
                    locations_stack.append(LocationSpecifier(LocationInitializer.NEW_KEY))
            case yaml.ValueToken | yaml.BlockEntryToken | yaml.FlowEntryToken:
                # A value to the previous key is coming.
                expected = ExpectedToken.VALUE
            case yaml.ScalarToken:
                # A token representing one string from the file.
                # Depending on what we are expecting, appropriate logic is applied.
                match expected:
                    case ExpectedToken.KEY:
                        assert isinstance(token, yaml.ScalarToken)
                        locations_stack[-1].set_key(token.value)
                        precise_location = tuple(
                            location.downgrade()
                            for location in (*locations_stack, LocationSpecifier.create_with_key("[key]"))
                        )
                        positions[precise_location] = (token.start_mark, token.end_mark)
                    case ExpectedToken.VALUE:
                        precise_location = tuple(location.downgrade() for location in locations_stack)
                        positions[precise_location] = (token.start_mark, token.end_mark)
                        # Were we in an implicit dictionary, it would end with the current token, thus we pop.
                        # Otherwise, we anticipate the next key.
                        if structures_stack[-1] is YamlStructureType.IMPLICIT_DICTIONARY:
                            locations_stack.pop()
                            structures_stack.pop()
                        else:
                            locations_stack[-1] = LocationSpecifier(LocationInitializer.NEW_KEY)
                    case ExpectedToken.ANY:
                        # We are a list element.
                        precise_location = tuple(location.downgrade() for location in locations_stack)
                        positions[precise_location] = (token.start_mark, token.end_mark)
                        if (
                            structures_stack
                            and structures_stack[-1] is YamlStructureType.IMPLICIT_LIST
                            and not isinstance(tokens[i + 1], yaml.BlockEntryToken)
                            and not isinstance(tokens[i + 1], yaml.FlowEntryToken)
                        ):
                            # We are the last element of an implicit list.
                            locations_stack.pop()
                            structures_stack.pop()
                expected = ExpectedToken.ANY
            case yaml.StreamStartToken | yaml.StreamEndToken:
                # Beginning and ending of the file.
                expected = ExpectedToken.ANY
            case _:
                # Disallowed yaml syntax.
                raise ConfigFileLoadError(
                    verbose_error_message(
                        path,
                        "This YAML syntax is not proper for a Quack Pack config file",
                        get_file_fragment(
                            path,
                            (token.start_mark.line, token.start_mark.column),
                            (token.end_mark.line, token.end_mark.column),
                        ),
                    )
                )
        # We add positions of the parentheses so that they can be detected as part of the problematic file fragment,
        # was the validation to fail.
        if isinstance(
            token,
            (
                yaml.FlowMappingStartToken,
                yaml.FlowMappingEndToken,
                yaml.FlowSequenceStartToken,
                yaml.FlowSequenceEndToken,
            ),
        ):
            precise_location = tuple(
                location.downgrade()
                for location in (
                    *locations_stack,
                    LocationSpecifier.create_with_key(f"!{parentheses_counter}"),
                )
            )
            positions[precise_location] = (token.start_mark, token.end_mark)
            parentheses_counter += 1
    return positions, deserialized_object


def proper_prefix_and_not_key(models: tuple[int | str, ...], candidates: tuple[int | str, ...]) -> bool:
    """
    Helper function for load_and_validate.
    Checks if model is a proper prefix of candidate and if candidate is not model concatenated with '[key]'.

    Args:
        models (tuple[int  |  str, ...]): tuple with which we check the candidate.
        candidates (tuple[int  |  str, ...]): candidate tuple.

    Returns:
        bool: Result of the comparison.
    """
    if len(candidates) <= len(models):
        return False
    for model, candidate in zip(models, candidates, strict=False):
        if model != candidate:
            return False
    return candidates[len(models)] != "[key]"


def load_and_validate[T: BaseModel](path: Path, expected_type: type[T]) -> T:
    """
    Deserializes a yaml file and validates the deserialized object with a given pydantic model.
    If something goes wrong, an error with a verbose, rich.console-compliant message
    (usually containing a fragment of the config file, which caused problems) is raised.

    Args:
        path (Path): path to the file to deserialize
        expected_type (type[T]): pydantic model, to which we should be able to deserialize the config file

    Raises:
        ConfigFileLoadError: custom error, raised when either deserialization or validation failed

    Returns:
        T: deserialized pydantic model
    """
    path = path.expanduser().resolve(strict=True)
    logger.debug(f"Attempting to deserialize '{expected_type.__name__}' from {path}")
    positions, parsed_data = strict_safe_load(path)
    try:
        return expected_type(**parsed_data)
    except TypeError:
        raise ConfigFileLoadError(
            verbose_error_message(path, "Your configuration file should deserialize to a dictionary")
        ) from None
    except ValidationError as e:
        error = e.errors()[0]
        non_union_error_keys = filter(lambda x: not isinstance(x, str) or x[0] != "!", error["loc"])
        non_union_error_keys = tuple(non_union_error_keys)

        # Firstly we check if the error is an 'extra_forbidden' error.
        # For some reason, pydantic does not append "[key]" to the error location.
        if error["type"] == "extra_forbidden":
            start_mark, end_mark = positions[*error["loc"], "[key]"]
            raise ConfigFileLoadError(
                verbose_error_message(
                    path,
                    error["msg"],
                    get_file_fragment(
                        path, (start_mark.line, start_mark.column), (end_mark.line, end_mark.column)
                    ),
                )
            ) from None

        # Secondly we check if the error location belongs to positions.
        if non_union_error_keys in positions:
            start_mark, end_mark = positions[non_union_error_keys]
            raise ConfigFileLoadError(
                verbose_error_message(
                    path,
                    error["msg"],
                    get_file_fragment(
                        path, (start_mark.line, start_mark.column), (end_mark.line, end_mark.column)
                    ),
                )
            ) from None

        # Now we look through all the positions with the same prefix as non_union_error_keys,
        # except (*non_union_error_keys, '[key]').
        # If there are any matches, we take the minimal and maximal lines and columns.
        # The fragment between them is what caused the error.
        # We added parentheses to positions, so the surrounding parentheses are also marked if necessary.
        min_line: int | None = None
        min_column: int | None = None
        max_line: int | None = None
        max_column: int | None = None
        for candidate_location, position in positions.items():
            if not proper_prefix_and_not_key(non_union_error_keys, candidate_location):
                continue
            start_line = position[0].line
            start_column = position[0].column
            end_line = position[1].line
            end_column = position[1].column
            if (
                min_line is None
                or start_line < min_line
                or (start_line == min_line and min_column is not None and start_column < min_column)
            ):
                min_line = start_line
                min_column = start_column
            if (
                max_line is None
                or end_line > max_line
                or (end_line == max_line and max_column is not None and end_column > max_column)
            ):
                max_line = end_line
                max_column = end_column
        if (
            min_line is not None
            and min_column is not None
            and max_line is not None
            and max_column is not None
        ):
            raise ConfigFileLoadError(
                verbose_error_message(
                    path,
                    error["msg"],
                    get_file_fragment(path, (min_line, min_column), (max_line, max_column)),
                )
            ) from None

        # No matches were found, so we check whether (*non_union_error_keys, "[key]") appears in positions.
        # This means an unspecified value of a key.
        if (*non_union_error_keys, "[key]") in positions:
            start_mark, end_mark = positions[*non_union_error_keys, "[key]"]
            raise ConfigFileLoadError(
                verbose_error_message(
                    path,
                    "Key should have a value assigned",
                    get_file_fragment(
                        path, (start_mark.line, start_mark.column), (end_mark.line, end_mark.column)
                    ),
                )
            ) from None

        # The only possibility left is a 'key_missing' error
        assert isinstance(non_union_error_keys[-1], str)
        missing_key = non_union_error_keys[-1]
        non_union_error_keys = non_union_error_keys[:-1]
        if not non_union_error_keys:
            # A top level section (like 'metadata') is missing
            raise ConfigFileLoadError(
                verbose_error_message(
                    path, f"Obligatory top level section <{missing_key}> absent in the file"
                )
            ) from None
        non_union_error_keys = (*non_union_error_keys, "[key]")
        start_mark, end_mark = positions[non_union_error_keys]
        raise ConfigFileLoadError(
            verbose_error_message(
                path,
                f"Obligatory field <{missing_key}> absent from this section",
                file_fragment=get_file_fragment(
                    path, (start_mark.line, start_mark.column), (end_mark.line, end_mark.column)
                ),
            )
        ) from None
