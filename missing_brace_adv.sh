#!/bin/bash

# This script recursively finds GridLAB-D .conf files using the GLPATH
# and checks them for unbalanced (mismatched) curly braces.
# This version is written for compatibility with older bash versions.

# --- SCRIPT START ---

# 1. Verify that the GLPATH environment variable is set.
if [ -z "$GLPATH" ]; then
    echo "Error: The GLPATH environment variable is not set."
    echo "Please ensure GLPATH is set to your GridLAB-D configuration and library paths."
    exit 1
fi

echo "Searching for unbalanced braces in .conf files found via GLPATH..."
echo "GLPATH is: $GLPATH"
echo "---"

# Use a simple string with separators to track seen files for portability.
seen_files_log=":" 

# Use a standard indexed array to hold the list of files to check.
files_to_check=()

# Function to find a file within the GLPATH and add it to our check-list if it's new.
add_file_if_unique() {
    local file_to_find=$1
    
    # Temporarily change the Internal Field Separator (IFS) to a colon to split GLPATH.
    local old_ifs=$IFS
    IFS=':'
    local paths=($GLPATH)
    IFS=$old_ifs

    for path in "${paths[@]}"; do
        local full_path="$path/$file_to_find"
        if [ -f "$full_path" ]; then
            # Resolve the real path to handle symlinks and relative paths consistently.
            local real_path
            real_path=$(cd "$(dirname "$full_path")" && pwd)/$(basename "$full_path")
            
            # Check if we've seen this file before by searching our log string.
            if ! echo "$seen_files_log" | grep -q ":$real_path:"; then
                files_to_check+=("$real_path")
                seen_files_log="${seen_files_log}${real_path}:"
                return 0 # File was found and added.
            fi
        fi
    done
    return 1 # File not found in any path.
}

# 2. Start the recursive search with the main configuration file.
add_file_if_unique "gridlabd.conf"

# 3. Iterate through the list of files, discovering any included files.
i=0
while [ "$i" -lt "${#files_to_check[@]}" ]; do
    current_file="${files_to_check[$i]}"
    
    # Find all '#include' and '#ifexist' lines and extract the quoted filename.
    includes=$(grep -E '^\s*#\s*(include|ifexist)\s+' "$current_file" | sed -E 's/.*["<]([^">]+)[">].*/\1/')

    for included_file in $includes; do
        add_file_if_unique "$included_file"
    done

    # Move to the next file in the array.
    ((i++))
done

echo "Scan complete. Found ${#files_to_check[@]} unique configuration file(s) to check."
echo "---"

# 4. Check each discovered file for a balanced number of braces.
found_error=0
for file in "${files_to_check[@]}"; do
    # Count occurrences of '{' and '}' in the file.
    opening=$(grep -o "{" "$file" | wc -l)
    closing=$(grep -o "}" "$file" | wc -l)

    if [ "$opening" -ne "$closing" ]; then
        echo "--> ERROR: Unbalanced braces found in: $file"
        echo "    Opening braces '{': $opening"
        echo "    Closing braces '}': $closing"
        found_error=1
    else
        echo "OK: $file (Braces: $opening)"
    fi
done

echo "---"
if [ "$found_error" -eq 0 ]; then
    echo "No unbalanced braces were found in any discovered configuration files."
else
    echo "Check the file(s) marked with ERROR above for a missing closing brace '}' in an object definition."
fi

# --- SCRIPT END ---
