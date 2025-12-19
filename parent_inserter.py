import re
import os
import sys

def find_object_end(lines, start):
    """
    Finds the ending line index of the current object, handling nesting.
    """
    level = 1  # Starting with the opening '{'
    j = start + 1
    while j < len(lines):
        stripped = lines[j].strip()
        if re.match(r'\s*object\s+[^\s{]+\s*\{?', lines[j]) and (re.sub(r'//.*$', '', lines[j]).rstrip().endswith('{') or (j+1 < len(lines) and lines[j+1].strip() == '{')):
            level += 1
        elif stripped.startswith('}') or stripped == '};':
            level -= 1
            if level == 0:
                return j
        j += 1
    return -1  # Error: no matching '}'

def modify_glm_file(input_filename):
    """
    Revises a GLM file to ensure that any nested '*assert', 'player', 'recorder', 
    or any other 'object' receives a 'parent' reference if nested. Assigns unique 
    names to nameless qualifying objects, using parent-based naming for asserts.
    Handles deep nesting and flexible brace placement.
    """
    try:
        with open(input_filename, 'r') as file:
            lines = file.read().splitlines()
    except FileNotFoundError:
        print(f"Error: Input file not found at '{input_filename}'")
        return

    modified_lines = []
    object_stack = []  # Stack of (type, name, start_index) tuples
    name_counters = {}
    parent_insert_count = 0
    name_insert_count = 0
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Detect object end
        if stripped.startswith('}') or stripped == '};':
            if object_stack:
                object_stack.pop()
            modified_lines.append(line)
            i += 1
            continue

        # Detect object start (handle '{' on same or next line)
        match = re.match(r'(\s*)object\s+([^\s{]+)', line)
        if match:
            current_indent = match.group(1)
            current_type = match.group(2)
            start_index = i

            # Remove comment for brace check
            comment_free = re.sub(r'//.*$', '', line).rstrip()

            # Check for '{' on this line or the next
            brace_line = i
            if not comment_free.endswith('{'):
                brace_line += 1
                if brace_line >= len(lines) or re.sub(r'//.*$', '', lines[brace_line]).strip() != '{':
                    modified_lines.append(line)
                    i += 1
                    continue

            # Append the opening lines to modified_lines
            modified_lines.append(line)
            if brace_line != i:
                modified_lines.append(lines[brace_line])
                i = brace_line + 1
            else:
                i += 1

            # Find the end of this object
            end = find_object_end(lines, brace_line)
            if end == -1:
                print(f"Warning: Unmatched object at line {start_index+1}")
                continue

            # Scan the body (brace_line+1 to end exclusive) for explicit name and parent
            determined_name = None
            has_explicit_name = False
            has_parent = False
            for j in range(brace_line + 1, end):
                check_line = lines[j].strip()
                check_line = re.sub(r'//.*$', '', check_line).strip()  # Remove inline comments
                if not check_line:
                    continue

                # Check for name (only set the first one found)
                name_match = re.match(r'name\s+["\']?([^"\';\s]+)["\']?', check_line)
                if name_match and not has_explicit_name:
                    determined_name = name_match.group(1)
                    has_explicit_name = True

                # Check for parent
                if check_line.startswith('parent '):
                    has_parent = True

            # Determine if needs parent/name
            is_player = current_type == 'player'
            is_recorder = current_type == 'recorder'
            is_assert = current_type == 'assert' or current_type.endswith('_assert')
            is_collector = current_type == 'collector'
            is_generic_object = current_type == 'object'
            needs_parent = is_player or is_recorder or is_assert or is_collector or is_generic_object

            # Get parent info from stack
            parent_name = object_stack[-1][1] if object_stack else None

            # Add parent reference if needed (for nested objects), without quotes
            if parent_name and needs_parent and not has_parent:
                insert_line = f"{current_indent}        parent {parent_name};"
                modified_lines.append(insert_line)
                parent_insert_count += 1
                print(f"Inserted parent reference '{parent_name}' for object '{current_type}' at line {len(modified_lines) + 1}")

            # Generate and insert name for nameless qualifying objects (include all types)
            if needs_parent and not has_explicit_name:
                key = f"{parent_name if parent_name else 'global'}_{current_type}"
                count = name_counters.get(key, 0)
                determined_name = f"{key}_{count}"
                name_counters[key] = count + 1
                insert_line = f"{current_indent}        name {determined_name};"
                modified_lines.append(insert_line)
                name_insert_count += 1
                print(f"Inserted name '{determined_name}' for object '{current_type}' at line {len(modified_lines) + 1}")

            # Set determined_name if not set (fallback for stack)
            if determined_name is None:
                key = f"{parent_name if parent_name else 'global'}_{current_type}"
                count = name_counters.get(key, 0)
                determined_name = f"{key}_{count}"
                name_counters[key] = count + 1  # Increment even for fallback

            # Push current object to stack
            object_stack.append((current_type, determined_name, start_index))
            continue

        # Append non-object lines
        modified_lines.append(line)
        i += 1

    # Write output
    base, ext = os.path.splitext(input_filename)
    output_filename = f"{base}_fix{ext}"
    
    with open(output_filename, 'w') as file:
        file.write('\n'.join(modified_lines) + '\n')
    
    print(f"Fixed GLM file written to: {output_filename}")
    # Print total insertions at the end
    print(f"Total parent references inserted: {parent_insert_count}")
    print(f"Total names inserted: {name_insert_count}")
    print(f"Grand total instances inserted: {parent_insert_count + name_insert_count}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
        modify_glm_file(input_file)
    else:
        print("Usage: python parent_inserter_fixed_v9.py <path_to_glm_file>")