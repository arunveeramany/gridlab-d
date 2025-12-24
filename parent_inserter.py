
#!/usr/bin/env python3
import re
import os
import sys
from typing import List, Tuple, Dict, Optional

def strip_comment(line: str) -> str:
    return re.sub(r"//.*$", "", line).rstrip()

def is_object_start(line: str) -> bool:
    return bool(re.match(r"^\s*object\s+[^{\s]+", strip_comment(line)))

def find_object_end(lines: List[str], brace_idx: int) -> int:
    depth = 1
    i = brace_idx + 1
    while i < len(lines) and depth > 0:
        s = strip_comment(lines[i]).strip()
        if s.endswith("{") or s == "{":
            depth += 1
        elif s.startswith("}") or s == "};":
            depth -= 1
        i += 1
    return i

used_names: set = set()
name_counters: Dict[str, int] = {}
summary: Dict[str, list] = {"names_added": [], "names_renamed": [], "parents_added": []}

def generate_unique_name(obj_type: str) -> str:
    n = name_counters.get(obj_type, 0) + 1
    cand = f"{obj_type}_{n}"
    while cand in used_names:
        n += 1
        cand = f"{obj_type}_{n}"
    name_counters[obj_type] = n
    used_names.add(cand)
    return cand

def parse_object(lines: List[str], start_idx: int, parent_name: Optional[str]):
    header_line = lines[start_idx]
    m = re.match(r"^(\s*)object\s+([^\s{]+)", strip_comment(header_line))
    indent = m.group(1)
    obj_type = m.group(2)

    brace_idx = start_idx
    if not strip_comment(header_line).endswith("{"):
        brace_idx += 1

    end_idx = find_object_end(lines, brace_idx)
    body_start = brace_idx + 1
    body_end = end_idx - 1

    # Scan body for name/parent
    has_name, has_parent, current_name = False, False, None
    for j in range(body_start, body_end):
        s = strip_comment(lines[j]).strip()
        if s.startswith("name ") and not has_name:
            nm = re.match(r"^name\s+([^;\s]+)", s)
            if nm:
                current_name = nm.group(1)
                has_name = True
        if s.startswith("parent "):
            has_parent = True

    # Determine body indentation (match existing content)
    body_indent = None
    for j in range(body_start, body_end):
        raw = lines[j]
        s = strip_comment(raw).strip()
        if s and not s.startswith("object") and not s.startswith("}"):
            body_indent = re.match(r"^(\s*)", raw).group(1)
            break
    if body_indent is None:
        body_indent = indent + "\t"  # fallback: one level deeper

    # Name resolution
    inserted_name = False
    if not has_name:
        current_name = generate_unique_name(obj_type)
        inserted_name = True
        summary["names_added"].append(current_name)
    else:
        if current_name in used_names:
            new_name = generate_unique_name(obj_type)
            summary["names_renamed"].append((current_name, new_name))
            current_name = new_name
            inserted_name = True
        else:
            used_names.add(current_name)

    # Parent insertion
    inserted_parent = False
    if parent_name and not has_parent:
        summary["parents_added"].append((current_name, parent_name))
        inserted_parent = True

    # Render object
    out = [header_line]
    if brace_idx != start_idx:
        out.append(lines[brace_idx])
    if inserted_name:
        out.append(f"{body_indent}name {current_name};")
    if inserted_parent:
        out.append(f"{body_indent}parent {parent_name};")

    i = body_start
    while i < body_end:
        s = strip_comment(lines[i]).strip()
        if is_object_start(s):
            child_lines, new_i, _ = parse_object(lines, i, current_name)
            out.extend(child_lines)
            i = new_i
            continue
        if s.startswith("name ") and inserted_name:
            i += 1
            continue
        if s.startswith("parent ") and inserted_parent:
            i += 1
            continue
        out.append(lines[i])
        i += 1

    out.append(lines[body_end])
    return out, end_idx, current_name

def process_file(path: str) -> str:
    with open(path, "r") as f:
        raw_lines = f.read().splitlines()

    result = []
    i = 0
    while i < len(raw_lines):
        s = strip_comment(raw_lines[i]).strip()
        if is_object_start(s):
            obj_lines, new_i, _ = parse_object(raw_lines, i, None)
            result.extend(obj_lines)
            i = new_i
        else:
            result.append(raw_lines[i])
            i += 1

    base, ext = os.path.splitext(path)
    out_path = f"{base}_fix{ext}"
    with open(out_path, "w") as f:
        f.write("\n".join(result) + "\n")
    return out_path

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python parent_inserter.py <path_to_glm_file>")
        sys.exit(1)
    out = process_file(sys.argv[1])
    print("Fixed GLM file written to:", out)

    # After writing out_path...
    print("Total names added:", len(summary["names_added"]))
    print("Total names renamed:", len(summary["names_renamed"]))
    print("Total parents added:", len(summary["parents_added"]))
    if summary["names_added"]:
        print("Names added:", ", ".join(summary["names_added"]))
    if summary["names_renamed"]:
        print("Names renamed:", ", ".join([f"{old}->{new}" for (old, new) in summary["names_renamed"]])) 
    if summary["parents_added"]:
        print("Parents added:", ", ".join([f"{child}->{parent}" for (child, parent) in summary["parents_added"]]))

