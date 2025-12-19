for file in *.conf; do
    opening=$(grep -o "{" "$file" | wc -l)
    closing=$(grep -o "}" "$file" | wc -l)
    if [ "$opening" -ne "$closing" ]; then
        echo "--> Unbalanced braces found in: $file"
        echo "    Opening braces '{': $opening"
        echo "    Closing braces '}': $closing"
    fi
done


