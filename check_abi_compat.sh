#!/bin/bash

# Paths to your binaries
EXE="./build-clang/bin/gridlabd"
LIB="./build-clang/lib/climate.dylib"

echo "🔍 Checking architecture and ABI compatibility..."
file "$EXE"
file "$LIB"

echo -e "\n🔍 Checking relocation model (PIE vs non-PIE)..."
otool -hv "$EXE" | grep -i PIE
otool -hv "$LIB" | grep -i PIE

echo -e "\n🔍 Checking dynamic symbols for OBJECT layout clues..."
nm -gU "$EXE" | grep -i object
nm -gU "$LIB" | grep -i object

echo -e "\n🔍 Checking for vtable symbols (C++ RTTI)..."
nm -gU "$LIB" | grep vtable
nm -gU "$EXE" | grep vtable

echo -e "\n✅ Done. Review the output above for mismatches."

