#!/usr/bin/env python3
"""Compile the native weapon databases into C++ without duplicating prices."""
import json
from pathlib import Path
import sys


def records(path):
    return [line.strip() for line in path.read_text().splitlines()
            if line.strip() and not line.lstrip().startswith("#")]


def generate(names_path, prices_path, output):
    words = records(names_path)
    numbers = records(prices_path)
    if len(words) % 2 or len(numbers) % 3 or len(words) // 2 != len(numbers) // 3:
        raise ValueError("Weapon databases have inconsistent record counts")
    rows = []
    for index in range(len(words) // 2):
        price, duration, reserved = map(int, numbers[index * 3:index * 3 + 3])
        if not 0 < price <= 65535 or not 0 <= duration <= 65535 or reserved != 0:
            raise ValueError("Invalid weapon pricing record " + str(index))
        name, description = words[index * 2:index * 2 + 2]
        rows.append("  {%s, %s, %d, %d}," % (
            json.dumps(name, ensure_ascii=True), json.dumps(description, ensure_ascii=True),
            price, duration))
    output.write_text("// Generated from usr/src/share/btweapons*.db; do not edit.\n"
                      "static const WeaponData weaponData[] = {\n" + "\n".join(rows) + "\n};\n")


if __name__ == "__main__":
    generate(*(Path(argument) for argument in sys.argv[1:]))
