@echo off
setlocal

rem ===== Set your variables here =====
set "GDL_VARIANT=gridlabd-cpp23-arm64"
set "VS_CONFIG=vs2022-x64-host-x64"
set "BUILD_TYPE=Debug"

rem ===== Derived paths =====
set "GDL_BASE=C:\Arun\conda_projects\%GDL_VARIANT%"
set "VS_OUT=%GDL_BASE%\out\build\%VS_CONFIG%"
set "GDL_BIN=%VS_OUT%\bin\%BUILD_TYPE%"
set "GDL_SHARE=%VS_OUT%\share"
set "GDL_CORE=%GDL_BASE%\gldcore"

rem ===== Define GLPATH using variables =====
set "GLPATH=%GDL_BIN%;%GDL_CORE%;%GDL_SHARE%"

rem ===== Update PATH (prepend GLPATH, keep existing PATH) =====
set "PATH=%GLPATH%;%PATH%"

rem ===== Show what we're using =====
echo [INFO] Using GridLAB-D variant: %GDL_VARIANT%
echo [INFO] VS build config: %VS_CONFIG%
echo [INFO] Build type: %BUILD_TYPE%
echo [INFO] Bin path: %GDL_BIN%
echo [INFO] GLPATH: %GLPATH%

rem ===== Verify resolution =====
where gridlabd.exe

rem ===== Launch shell =====
"%ComSpec%" /k

