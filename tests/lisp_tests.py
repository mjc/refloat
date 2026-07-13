#!/usr/bin/env python3

from __future__ import annotations

import unittest
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Sym:
    name: str


@dataclass(frozen=True)
class Str:
    value: str


@dataclass(frozen=True)
class Token:
    value: Any
    line: int


class LocatedList(list[Any]):
    def __init__(self, values: list[Any], line: int, source: str) -> None:
        super().__init__(values)
        self.line = line
        self.source = source


@dataclass(frozen=True)
class Block:
    forms: list[Any]
    line: int
    source: str


class LispError(Exception):
    pass


class StopLoop(Exception):
    pass


def _tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    i = 0
    while i < len(source):
        c = source[i]
        line = source.count("\n", 0, i) + 1
        if c.isspace():
            i += 1
        elif c == ";":
            while i < len(source) and source[i] != "\n":
                i += 1
        elif c in "(){}":
            tokens.append(Token(c, line))
            i += 1
        elif c == '"':
            i += 1
            value = []
            while i < len(source):
                if source[i] == "\\" and i + 1 < len(source):
                    value.append(source[i + 1])
                    i += 2
                elif source[i] == '"':
                    i += 1
                    break
                else:
                    value.append(source[i])
                    i += 1
            else:
                raise LispError("unterminated string")
            tokens.append(Token(Str("".join(value)), line))
        elif c == "'":
            i += 1
            start = i
            while i < len(source) and not source[i].isspace() and source[i] not in "(){}":
                i += 1
            tokens.append(Token([Sym("quote"), Sym(source[start:i])], line))
        else:
            start = i
            while i < len(source) and not source[i].isspace() and source[i] not in "(){}":
                i += 1
            atom = source[start:i]
            try:
                tokens.append(Token(int(atom), line))
            except ValueError:
                try:
                    tokens.append(Token(float(atom), line))
                except ValueError:
                    tokens.append(Token(Sym(atom), line))
    return tokens


def _parse_one(tokens: list[Token], source: str, index: int = 0) -> tuple[Any, int]:
    token = tokens[index]
    value = token.value
    if value == "(":
        index += 1
        forms = []
        while index < len(tokens) and tokens[index].value != ")":
            form, index = _parse_one(tokens, source, index)
            forms.append(form)
        if index >= len(tokens):
            raise LispError("missing )")
        return LocatedList(forms, token.line, source), index + 1
    if value == "{":
        index += 1
        forms = []
        while index < len(tokens) and tokens[index].value != "}":
            form, index = _parse_one(tokens, source, index)
            forms.append(form)
        if index >= len(tokens):
            raise LispError("missing }")
        return Block(forms, token.line, source), index + 1
    if value in (")", "}"):
        raise LispError(f"unexpected {value}")
    return value, index + 1


def _collect_source_lines(forms: list[Any], lines: set[int]) -> None:
    for form in forms:
        if isinstance(form, LocatedList):
            lines.add(form.line)
            _collect_source_lines(form, lines)
        elif isinstance(form, Block):
            lines.add(form.line)
            _collect_source_lines(form.forms, lines)


def parse_program(source: str, source_name: str) -> list[Any]:
    tokens = _tokenize(source)
    forms = []
    index = 0
    while index < len(tokens):
        form, index = _parse_one(tokens, source_name, index)
        forms.append(form)
    lines = COVERAGE_TOTAL.setdefault(source_name, set())
    _collect_source_lines(forms, lines)
    return forms


COVERAGE_TOTAL: dict[str, set[int]] = {}
COVERAGE_HIT: dict[str, set[int]] = {}


class Runtime:
    def __init__(self) -> None:
        self.imports: list[tuple[str, str]] = []
        self.loaded_native_libs: list[str] = []
        self.fw_ver = [6, 5, 0]
        self.ext_bms_enabled = True
        self.ext_fw_versions: list[list[int]] = []
        self.spawned: list[tuple[str, int, str]] = []
        self.prints: list[str] = []
        self.programs: dict[str, list[Any]] = {}
        self.bms_values: dict[str, Any] = {}
        self.bms_sequence: list[dict[str, Any]] = []
        self.bms_publications: list[tuple[float, float, int, int, int, float]] = []
        self.bms_status_requests = 0
        self.sleeps: list[float] = []
        self.max_sleeps = 1

    def import_file(self, path: str, alias: str) -> str:
        self.imports.append((path, alias))
        if path == "bms.lisp":
            self.programs[alias] = parse_program((ROOT / "lisp" / "bms.lisp").read_text(), "lisp/bms.lisp")
        return alias

    def record_coverage(self, source: str, line: int) -> None:
        COVERAGE_HIT.setdefault(source, set()).add(line)

    def load_native_lib(self, alias: str) -> None:
        self.loaded_native_libs.append(alias)

    def get_bms_val(self, key: str, *args: Any) -> Any:
        lookup = f"{key}:{args[0]}" if args else key
        if self.bms_sequence:
            current = self.bms_sequence[min(len(self.sleeps), len(self.bms_sequence) - 1)]
            if lookup in current:
                value = current[lookup]
            elif key in current:
                value = current[key]
            else:
                raise LispError(f"missing bms value {lookup}")
        elif lookup in self.bms_values:
            value = self.bms_values[lookup]
        elif key in self.bms_values:
            value = self.bms_values[key]
        else:
            raise LispError(f"missing bms value {lookup}")
        if isinstance(value, Exception):
            raise value
        return value

    def ext_bms(self, *args: Any) -> bool:
        if not args:
            self.bms_status_requests += 1
            return self.ext_bms_enabled
        self.bms_publications.append(
            (float(args[0]), float(args[1]), int(args[2]), int(args[3]), int(args[4]), float(args[5]))
        )
        return self.ext_bms_enabled

    def sleep(self, seconds: float) -> None:
        self.sleeps.append(seconds)
        if len(self.sleeps) >= self.max_sleeps:
            raise StopLoop()


class Env(dict[str, Any]):
    def __init__(self, runtime: Runtime, parent: Env | None = None) -> None:
        super().__init__()
        self.runtime = runtime
        self.parent = parent

    def find(self, name: str) -> Env:
        if name in self:
            return self
        if self.parent is not None:
            return self.parent.find(name)
        raise LispError(f"unknown symbol {name}")

    def get(self, name: str) -> Any:
        if name == "t":
            return True
        if name == "nil":
            return None
        return self.find(name)[name]

    def set_existing(self, name: str, value: Any) -> None:
        self.find(name)[name] = value


def _truthy(value: Any) -> bool:
    return value not in (None, False)


def _eval_block(block: Block, env: Env) -> Any:
    result = None
    for form in block.forms:
        result = eval_lisp(form, env)
    return result


def _eval_sequence(forms: list[Any], env: Env) -> Any:
    result = None
    for form in forms:
        result = eval_lisp(form, env)
    return result


def eval_lisp(form: Any, env: Env) -> Any:
    if isinstance(form, (LocatedList, Block)):
        env.runtime.record_coverage(form.source, form.line)
    if isinstance(form, Sym):
        return env.get(form.name)
    if isinstance(form, Str):
        return form.value
    if isinstance(form, (int, float)):
        return form
    if isinstance(form, Block):
        return _eval_block(form, env)
    if not isinstance(form, list):
        raise LispError(f"cannot evaluate {form!r}")
    if not form:
        return None

    op = form[0]
    if not isinstance(op, Sym):
        raise LispError(f"invalid call {form!r}")
    name = op.name

    if name == "quote":
        quoted = form[1]
        if isinstance(quoted, Sym):
            return quoted.name
        return quoted
    if name in ("define", "var"):
        key = form[1].name
        env[key] = eval_lisp(form[2], env)
        return env[key]
    if name == "setq":
        key = form[1].name
        value = eval_lisp(form[2], env)
        env.set_existing(key, value)
        return value
    if name == "defun":
        function_name = form[1].name
        params = [param.name for param in form[2]]
        body = form[3]

        def call(args: list[Any]) -> Any:
            child = Env(env.runtime, env)
            for param, arg in zip(params, args):
                child[param] = arg
            return eval_lisp(body, child)

        env[function_name] = call
        return call
    if name == "if":
        condition = eval_lisp(form[1], env)
        if _truthy(condition):
            return eval_lisp(form[2], env)
        if len(form) > 3:
            return eval_lisp(form[3], env)
        return None
    if name == "progn":
        return _eval_sequence(form[1:], env)
    if name == "import":
        path = eval_lisp(form[1], env)
        alias = eval_lisp(form[2], env)
        imported = env.runtime.import_file(path, alias)
        env[alias] = imported
        return imported
    if name == "apply":
        fn = eval_lisp(form[1], env)
        values = eval_lisp(form[2], env)
        return fn(list(values))
    if name == "and":
        result = True
        for arg in form[1:]:
            result = eval_lisp(arg, env)
            if not _truthy(result):
                return None
        return result
    if name == "or":
        for arg in form[1:]:
            result = eval_lisp(arg, env)
            if _truthy(result):
                return result
        return None
    if name == "trap":
        try:
            return ["exit-ok", eval_lisp(form[1], env)]
        except Exception as exc:
            return ["exit-error", exc]
    if name == "loopwhile":
        try:
            while _truthy(eval_lisp(form[1], env)):
                eval_lisp(form[2], env)
        except StopLoop:
            return None
    if name == "looprange":
        key = form[1].name
        start = int(eval_lisp(form[2], env))
        end = int(eval_lisp(form[3], env))
        child = Env(env.runtime, env)
        for i in range(start, end):
            child[key] = i
            eval_lisp(form[4], child)
        return None

    args = [eval_lisp(arg, env) for arg in form[1:]]
    if name == "load-native-lib":
        return env.runtime.load_native_lib(args[0])
    if name == "sysinfo":
        if args[0] != "fw-ver":
            raise LispError(f"unsupported sysinfo {args[0]}")
        return env.runtime.fw_ver
    if name == "ext-set-fw-version":
        env.runtime.ext_fw_versions.append(list(args))
        return True
    if name == "ext-bms":
        return env.runtime.ext_bms(*args)
    if name == "read-eval-program":
        return _eval_sequence(env.runtime.programs[args[0]], env)
    if name == "spawn":
        env.runtime.spawned.append((args[0], int(args[1]), form[3].name))
        return True
    if name == "print":
        env.runtime.prints.append(args[0])
        return None
    if name == "get-bms-val":
        return env.runtime.get_bms_val(*args)
    if name == "sleep":
        return env.runtime.sleep(float(args[0]))
    if name == "first":
        return args[0][0]
    if name == "second":
        return args[0][1]
    if name == "eq":
        return args[0] == args[1]
    if name == "=":
        return args[0] == args[1]
    if name == ">":
        return args[0] > args[1]
    if name == "<":
        return args[0] < args[1]
    if name == ">=":
        return args[0] >= args[1]
    if name == "<=":
        return args[0] <= args[1]
    fn = env.get(name)
    if callable(fn):
        return fn(args)
    raise LispError(f"unsupported function {name}")


def run_package(runtime: Runtime) -> Env:
    env = Env(runtime)
    env["ext-set-fw-version"] = lambda args: runtime.ext_fw_versions.append(list(args)) or True
    env["ext-bms"] = lambda args: runtime.ext_bms(*args)
    _eval_sequence(parse_program((ROOT / "lisp" / "package.lisp").read_text(), "lisp/package.lisp"), env)
    return env


def run_bms_loop(runtime: Runtime) -> None:
    env = Env(runtime)
    env["ext-bms"] = lambda args: runtime.ext_bms(*args)
    _eval_sequence(parse_program((ROOT / "lisp" / "bms.lisp").read_text(), "lisp/bms.lisp"), env)
    env.get("bms-loop")([])


class PackageLispBehaviorTests(unittest.TestCase):
    def test_startup_loads_native_lib_and_skips_bms_when_disabled(self) -> None:
        runtime = Runtime()
        runtime.ext_bms_enabled = False

        run_package(runtime)

        self.assertEqual(runtime.imports, [("src/package_lib.bin", "package-lib")])
        self.assertEqual(runtime.loaded_native_libs, ["package-lib"])
        self.assertEqual(runtime.ext_fw_versions, [[6, 5, 0]])
        self.assertEqual(runtime.spawned, [])
        self.assertEqual(runtime.prints, [])

    def test_startup_rejects_unsupported_bms_firmware_without_spawn(self) -> None:
        runtime = Runtime()
        runtime.fw_ver = [6, 4, 99]

        run_package(runtime)

        self.assertEqual(runtime.ext_fw_versions, [[6, 4, 99]])
        self.assertEqual(runtime.spawned, [])
        self.assertEqual(
            runtime.prints,
            ["[refloat] BMS Integration: Unsupported firmware version, 6.05+ required."],
        )

    def test_startup_loads_and_spawns_bms_loop_on_supported_firmware(self) -> None:
        runtime = Runtime()
        runtime.fw_ver = [7, 0, 0]

        run_package(runtime)

        self.assertEqual(
            runtime.imports,
            [("src/package_lib.bin", "package-lib"), ("bms.lisp", "bms")],
        )
        self.assertEqual(runtime.spawned, [("Refloat BMS", 50, "bms-loop")])


class BmsLispBehaviorTests(unittest.TestCase):
    def test_supported_cell_helpers_publish_min_max_and_data_version_one_temperatures(self) -> None:
        runtime = Runtime()
        runtime.bms_values = {
            "bms-v-cell-min": 3.12,
            "bms-can-id": 42,
            "bms-msg-age": 0.4,
            "bms-temp-cell-max": 36,
            "bms-data-version": 1,
            "bms-temps-adc:1": 25,
            "bms-temps-adc:3": 44,
            "bms-v-cell-max": 4.09,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications, [(3.12, 4.09, 25, 36, 44, 0.4)])
        self.assertEqual(runtime.sleeps, [0.2])

    def test_cell_count_fallback_scans_cells_when_min_max_helpers_are_missing(self) -> None:
        runtime = Runtime()
        runtime.bms_values = {
            "bms-v-cell-min": LispError("unsupported"),
            "bms-can-id": 42,
            "bms-msg-age": 1.25,
            "bms-temp-cell-max": 33,
            "bms-cell-num": 4,
            "bms-v-cell:0": 3.8,
            "bms-v-cell:1": 3.7,
            "bms-v-cell:2": 4.1,
            "bms-v-cell:3": 3.9,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications, [(3.7, 4.1, 33, 33, -281, 1.25)])
        self.assertEqual(runtime.sleeps, [0.2])

    def test_missing_can_id_does_not_publish_bms_sample(self) -> None:
        runtime = Runtime()
        runtime.bms_values = {
            "bms-v-cell-min": 3.2,
            "bms-can-id": -1,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications, [])
        self.assertEqual(runtime.sleeps, [0.2])

    def test_status_request_happens_before_can_id_gate(self) -> None:
        """A configured smart BMS is polled before its first CAN sample exists."""
        runtime = Runtime()
        # ext_bms_enabled models Refloat's BMS integration being configured.
        # A missing CAN id is the normal pre-sample state, not a no-smart-BMS
        # configuration; that path is covered by the disabled test below.
        runtime.bms_values = {
            "bms-v-cell-min": 3.2,
            "bms-can-id": -1,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_status_requests, 1)

    def test_disabled_bms_extension_does_not_publish_bms_sample(self) -> None:
        runtime = Runtime()
        # No smart BMS is the disabled integration path. The -1 CAN-id state
        # is normal here and must not become a connection/publication fault.
        runtime.ext_bms_enabled = False
        runtime.bms_values = {
            "bms-v-cell-min": 3.2,
            "bms-can-id": 42,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications, [])
        self.assertEqual(runtime.sleeps, [0.2])

    def test_zero_cell_fallback_does_not_publish_stale_voltage_from_previous_sample(self) -> None:
        """Red test: a zero-cell fallback sample should not publish stale voltages.

        The current script keeps v-min/v-max outside the polling iteration. After a valid fallback sample,
        a later zero-cell sample can skip the cell scan and still call ext-bms with the previous voltages.
        """
        runtime = Runtime()
        runtime.max_sleeps = 2
        runtime.bms_sequence = [
            {
                "bms-v-cell-min": LispError("unsupported"),
                "bms-can-id": 42,
                "bms-msg-age": 0.1,
                "bms-temp-cell-max": 31,
                "bms-cell-num": 2,
                "bms-v-cell:0": 3.5,
                "bms-v-cell:1": 4.0,
            },
            {
                "bms-v-cell-min": LispError("unsupported"),
                "bms-can-id": 42,
                "bms-msg-age": 0.3,
                "bms-temp-cell-max": 32,
                "bms-cell-num": 0,
            },
        ]

        run_bms_loop(runtime)

        self.assertEqual(len(runtime.bms_publications), 1)

    def test_data_version_zero_sample_does_not_publish_stale_fet_temperature(self) -> None:
        """Red test: non-version-1 samples should not retain the previous FET temperature.

        temp-fet is initialized once before the loop and only refreshed for data-version 1 samples. A later
        supported-helper sample with another data version can publish the old FET temperature as if it were fresh.
        """
        runtime = Runtime()
        runtime.max_sleeps = 2
        runtime.bms_sequence = [
            {
                "bms-v-cell-min": 3.1,
                "bms-can-id": 42,
                "bms-msg-age": 0.1,
                "bms-temp-cell-max": 36,
                "bms-data-version": 1,
                "bms-temps-adc:1": 25,
                "bms-temps-adc:3": 44,
                "bms-v-cell-max": 4.0,
            },
            {
                "bms-v-cell-min": 3.2,
                "bms-can-id": 42,
                "bms-msg-age": 0.2,
                "bms-temp-cell-max": 37,
                "bms-data-version": 0,
                "bms-v-cell-max": 4.1,
            },
        ]

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications[1], (3.2, 4.1, 37, 37, -281, 0.2))

    def test_mid_sample_bms_read_failure_suppresses_partial_publication_and_keeps_polling(self) -> None:
        """Red test: a partial BMS sample should not abort the polling loop or publish stale data.

        The current script traps only the initial capability probe. Failures after CAN ID/msg-age are read can
        abort bms-loop before sleep, leaving future samples unpolled instead of suppressing just the bad sample.
        """
        runtime = Runtime()
        runtime.bms_values = {
            "bms-v-cell-min": 3.1,
            "bms-can-id": 42,
            "bms-msg-age": 0.1,
            "bms-temp-cell-max": LispError("missing temperature"),
            "bms-v-cell-max": 4.0,
        }

        run_bms_loop(runtime)

        self.assertEqual(runtime.bms_publications, [])
        self.assertEqual(runtime.sleeps, [0.2])


def print_coverage() -> None:
    total = sum(len(lines) for lines in COVERAGE_TOTAL.values())
    hit = sum(len(COVERAGE_HIT.get(source, set())) for source in COVERAGE_TOTAL)
    percent = 100.0 if total == 0 else hit * 100.0 / total
    print(f"lisp coverage: {hit}/{total} executable source lines ({percent:.1f}%)")


if __name__ == "__main__":
    result = unittest.main(exit=False)
    print_coverage()
    raise SystemExit(not result.result.wasSuccessful())
