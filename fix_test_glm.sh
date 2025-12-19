
#!/bin/bash

# Step 0: Determine which E line to use
N=${1:-1}  # Default to 1st E line if no argument is provided

suite="generators"
err_type="E"   #manually change ^X

# Loop through N = 1 to 10
for N in {1..40}; do
  echo "🔁 Starting iteration N=$N"

  echo "🔍 Step 1: Reading validate.txt to find the ${N}th line starting with '${err_type}'..."

  VALIDATE_FILE="/Users/arun.veeramany/Arun/conda_projects/gridlabd-cpp23/${suite}/validate.txt"

  # Use awk to get the nth line starting with 'E' and extract the .glm path
  GLM_PATH=$(awk -v n="$N" '/^E/ {count++; if (count == n) print $3}' "$VALIDATE_FILE")

  if [ -z "$GLM_PATH" ]; then
    echo "❌ Error: Could not find the ${N}th .glm path in validate.txt."
    exit 0
  fi
  echo "✅ Found .glm path: $GLM_PATH"


if [[ "$GLM_PATH" == *"_fix.glm" ]]; then
  echo "⏭️ Skipping iteration N=$N because $GLM_PATH already has '_fix.glm'"
  continue
fi

  # Step 2: Strip .glm extension to get the directory path
  GLM_DIR="${GLM_PATH%.glm}"
  GLM_FILE=$(basename "$GLM_PATH")

  echo "📁 Changing to directory: $GLM_DIR"
  cd "$GLM_DIR" || { echo "❌ Error: Failed to change to directory $GLM_DIR"; exit 1; }
  echo "✅ Changed to directory: $(pwd)"

  # Step 3: Run the parent_inserter.py script
  echo "🛠️ Running parent_inserter.py on $GLM_FILE..."
  python3 ../../../parent_inserter.py "$GLM_FILE"
  if [ $? -ne 0 ]; then
    echo "❌ Error: parent_inserter.py failed."
    exit 1
  fi
  echo "✅ parent_inserter.py completed"

  # Step 4: Construct the fixed filename
  FIXED_FILE="${GLM_FILE%.glm}_fix.glm"
  echo "📄 Fixed GLM file: $FIXED_FILE"

  # Step 5: Run GridLAB-D on the fixed file
  GRIDLABD_BIN="/Users/arun.veeramany/Arun/conda_projects/gridlabd-cpp23/build-clang/bin/gridlabd"
  echo "⚡ Running GridLAB-D on $FIXED_FILE..."
  "$GRIDLABD_BIN" "$FIXED_FILE"
  EXIT_CODE=$?

  # Step 6: Check the exit code
  if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ GridLAB-D ran successfully with exit code 0"
    echo "📤 Copying $FIXED_FILE to parent directory..."
    cp "$FIXED_FILE" ../
    echo "✅ File copied to parent directory"


  # Change to parent directory
    cd ../ || { echo "❌ Error: Failed to change to parent directory"; exit 1; }
    echo "📁 Changed to parent directory: $(pwd)"

    # Rename original GLM file by inserting "opt_" after "test_"
    ORIGINAL_FILE="$GLM_FILE"
    RENAMED_FILE=$(echo "$ORIGINAL_FILE" | sed 's/test_/test_opt_/')
    if [ "$ORIGINAL_FILE" != "$RENAMED_FILE" ]; then
      mv "$ORIGINAL_FILE" "$RENAMED_FILE"
      echo "✅ Renamed $ORIGINAL_FILE to $RENAMED_FILE"
      cd ../../ || { echo "❌ Error: Failed to change back to original directory"; exit 1; }
    else
      echo "⚠️ Could not rename $ORIGINAL_FILE — 'test_' not found in filename"
    fi

  else
    echo "❌ GridLAB-D failed with exit code $EXIT_CODE. File not copied."
  fi
done


#Final step: Run GridLAB-D validation
# echo "🧪 Running GridLAB-D validation..."
# cd /Users/arun.veeramany/Arun/conda_projects/gridlabd-cpp23/residential || { echo "❌ Failed to change to residential directory"; exit 1; }
# "$GRIDLABD_BIN" --validate
# cd ../ || { echo "❌ Failed to change back to original directory"; exit 1; }
