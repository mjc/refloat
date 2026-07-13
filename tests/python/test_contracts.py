#!/usr/bin/env python3
"""Small executable checks for repo-owned build and config contracts."""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

import rjsmin  # noqa: E402


def check_config_wire_ranges():
    params = ET.parse(ROOT / "src/conf/settings.xml").getroot().find("Params")
    wire_ranges = {
        "1": (0, 255),
        "2": (-128, 127),
        "3": (0, 65535),
        "7": (-32768, 32767),
    }
    # These existing ERPM fields are intentionally tracked separately: changing
    # their width is a compatibility decision, not part of this boundary pass.
    known_erpm_uint16 = {
        "fault_adc_half_erpm",
        "tiltback_constant_erpm",
        "tiltback_variable_erpm",
        "turntilt_start_erpm",
        "turntilt_erpm_boost_end",
    }

    for param in params:
        tx = param.findtext("vTx")
        if tx not in wire_ranges or param.tag in known_erpm_uint16:
            continue
        low, high = wire_ranges[tx]
        scale = float(param.findtext("vTxDoubleScale", "1"))
        for tag in ("minDouble", "maxDouble", "valDouble", "minInt", "maxInt", "valInt"):
            text = param.findtext(tag)
            if text is None:
                continue
            encoded = float(text) * scale
            if not low <= encoded <= high:
                raise AssertionError(
                    f"{param.tag} {tag}={text} exceeds vTx {tx} range after scale {scale:g}"
                )


def main():
    cases = {
        "var x = 1 + 2; // comment\n": "var x=1+2;",
        "var s = 'a // b'; /* comment */ var t = `x ${1 + 2}`;":
            "var s='a // b';var t=`x ${1 + 2}`;",
        "var r = /a + b/g;": "var r=/a + b/g;",
        "a + +b; a - -b;": "a+ +b;a- -b;",
    }

    for source, expected in cases.items():
        actual = rjsmin.jsmin(source)
        if actual != expected:
            raise AssertionError(f"rjsmin mismatch: {source!r} -> {actual!r}")

    bang = "/*! keep */ var x = 1;"
    if rjsmin.jsmin(bang, keep_bang_comments=True) != "/*! keep */var x=1;":
        raise AssertionError("rjsmin did not preserve a bang comment")

    check_config_wire_ranges()

    print(f"python contract checks: {len(cases) + 2} passed")


if __name__ == "__main__":
    main()
