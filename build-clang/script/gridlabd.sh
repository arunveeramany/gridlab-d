#!/usr/bin/env bash

# WARNING: This is a CMake generated file, modifications to this file will be overwritten.

export CC="/usr/bin/clang"
export CXX="/usr/bin/clang++"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

bindir="${SCRIPT_DIR}"
pkglibdir="${SCRIPT_DIR}/../lib"
HAS_MINGW=""
IS_MAC="yes"

if test "x$IS_MAC" = "xyes"; then
  export DYLD_LIBRARY_PATH=$pkglibdir
  export SDKROOT="$(xcrun --show-sdk-path)"
fi

#if test "x$GLPATH" = x; then
#  export GLPATH="$pkglibdir:$pkgdatadir"
#else
#  export GLPATH="$pkglibdir:$pkgdatadir:$GLPATH"
#fi
#
#if test "x$TERM" = "xcygwin"; then
#  export CXXFLAGS="-I${pkgdatadir} -fPIC $CXXFLAGS"
#else
#  export CXXFLAGS="-I$pkgdatadir -fPIC $CXXFLAGS"
#fi
#
#if test "x" != "xyes"; then # Generated test for WIN32
#  export CXXFLAGS="$CXXFLAGS -fPIC"
#fi

export CXXFLAGS="-Werror=return-type -Werror=write-strings $<$<CONFIG:Debug>:-g -O0>"

if test "x$GRIDLABD_DEBUG" = "x"; then
  if test "x$HAS_MINGW" = "xyes"; then
    "$bindir/gridlabd.exe" "$@"
  else
    "$bindir/gridlabd" "$@"
  fi
else
  if test "x$HAS_MINGW" = "xyes"; then
    "$GRIDLABD_DEBUG" "$bindir/gridlabd.exe"
  else
    "$GRIDLABD_DEBUG" "$bindir/gridlabd"
  fi
fi
