#!/usr/bin/env python3
"""Generate a C++ header that embeds a binary asset as a byte array.

Usage: embed_asset.py <input> <output_header> <symbol>
"""
import os
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    in_path, out_path, symbol = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(in_path, "rb") as f:
        data = f.read()

    os.makedirs(os.path.dirname(out_path), exist_ok=True)

    # Build in memory, write once.
    parts = [
        "// Auto-generated. Do not edit.\n",
        "#pragma once\n",
        "namespace mp::embedded {\n",
        f"inline constexpr unsigned char {symbol}[] = {{\n",
    ]
    line_buf = []
    for i, b in enumerate(data):
        line_buf.append(f"0x{b:02x},")
        if (i + 1) % 16 == 0:
            parts.append("    " + "".join(line_buf) + "\n")
            line_buf = []
    if line_buf:
        parts.append("    " + "".join(line_buf) + "\n")
    parts.append("};\n")
    parts.append(f"inline constexpr unsigned int {symbol}_len = {len(data)};\n")
    parts.append("} // namespace mp::embedded\n")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("".join(parts))

    return 0


if __name__ == "__main__":
    sys.exit(main())
