@echo off
setlocal EnableDelayedExpansion
title ExtraGraphAnalyst Build

echo.
echo =============================================
echo  ExtraGraphAnalyst - Build Script
echo =============================================
echo.

:: -----------------------------------------------
:: 1. Find Python (py launcher, then python)
:: -----------------------------------------------
echo [1/4] Finding Python...

set PY=
py --version >nul 2>&1
if not errorlevel 1 set PY=py

if not defined PY (
    python --version >nul 2>&1
    if not errorlevel 1 set PY=python
)

if not defined PY (
    echo ERROR: Python not found.
    echo Install from https://www.python.org/downloads/
    pause
    exit /b 1
)

for /f "tokens=*" %%v in ('!PY! --version 2^>^&1') do echo   %%v  [!PY!]
echo.

:: -----------------------------------------------
:: 2. Find CMake
:: -----------------------------------------------
echo [2/4] Finding CMake...

set CM=

where cmake >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('where cmake') do set CM=%%i
)

if not defined CM if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set CM=%ProgramFiles%\CMake\bin\cmake.exe
)

if not defined CM if exist "C:\msys64\ucrt64\bin\cmake.exe" (
    set CM=C:\msys64\ucrt64\bin\cmake.exe
)

if not defined CM if exist "C:\msys64\mingw64\bin\cmake.exe" (
    set CM=C:\msys64\mingw64\bin\cmake.exe
)

if not defined CM (
    :: cmake via pip lands in Python Scripts folder
    for /f "tokens=*" %%s in ('!PY! -c "import sys,os; print(os.path.dirname(sys.executable))" 2^>nul') do (
        if exist "%%s\Scripts\cmake.exe" set CM=%%s\Scripts\cmake.exe
        if exist "%%s\cmake.exe"         set CM=%%s\cmake.exe
    )
)

if not defined CM (
    echo   cmake not in PATH, trying pip install...
    !PY! -m pip install cmake -q
    for /f "tokens=*" %%s in ('!PY! -c "import sys,os; print(os.path.dirname(sys.executable))" 2^>nul') do (
        if exist "%%s\Scripts\cmake.exe" set CM=%%s\Scripts\cmake.exe
    )
    where cmake >nul 2>&1
    if not errorlevel 1 (
        for /f "tokens=*" %%i in ('where cmake') do set CM=%%i
    )
)

if not defined CM (
    echo ERROR: CMake not found.
    echo Run:  !PY! -m pip install cmake
    echo Then run build.bat again.
    pause
    exit /b 1
)

echo   Found: !CM!
echo.

:: -----------------------------------------------
:: 3. Find g++ (MSYS2 / MinGW)
:: -----------------------------------------------
echo [3/4] Finding g++...

set GPP=
set MINGWBIN=

where g++ >nul 2>&1
if not errorlevel 1 (
    for /f "tokens=*" %%i in ('where g++') do set GPP=%%i
)

if not defined GPP if exist "C:\msys64\ucrt64\bin\g++.exe" (
    set GPP=C:\msys64\ucrt64\bin\g++.exe
    set MINGWBIN=C:\msys64\ucrt64\bin
)

if not defined GPP if exist "C:\msys64\mingw64\bin\g++.exe" (
    set GPP=C:\msys64\mingw64\bin\g++.exe
    set MINGWBIN=C:\msys64\mingw64\bin
)

if not defined GPP if exist "C:\msys2\ucrt64\bin\g++.exe" (
    set GPP=C:\msys2\ucrt64\bin\g++.exe
    set MINGWBIN=C:\msys2\ucrt64\bin
)

if not defined GPP if exist "C:\TDM-GCC-64\bin\g++.exe" (
    set GPP=C:\TDM-GCC-64\bin\g++.exe
    set MINGWBIN=C:\TDM-GCC-64\bin
)

if not defined GPP if exist "C:\mingw64\bin\g++.exe" (
    set GPP=C:\mingw64\bin\g++.exe
    set MINGWBIN=C:\mingw64\bin
)

if not defined GPP (
    echo ERROR: g++ not found.
    echo.
    echo Open the MSYS2 UCRT64 terminal and run:
    echo   pacman -S mingw-w64-ucrt-x86_64-gcc
    echo.
    echo Then run build.bat again.
    pause
    exit /b 1
)

if defined MINGWBIN (
    set "PATH=!MINGWBIN!;!PATH!"
)

for /f "tokens=*" %%v in ('"!GPP!" --version 2^>^&1') do echo   %%v
echo.

:: -----------------------------------------------
:: 4. Install pip packages + build
:: -----------------------------------------------
echo [4/4] Installing Python packages and building...

!PY! -m pip install -q pandas numpy statsmodels scikit-learn yfinance openpyxl
echo   Packages ready.
echo.

if exist build rmdir /s /q build
mkdir build
cd build

:: Pick generator: Ninja if available, else MinGW Makefiles
set GEN=MinGW Makefiles
where ninja >nul 2>&1
if not errorlevel 1 set GEN=Ninja
if exist "C:\msys64\ucrt64\bin\ninja.exe"  set GEN=Ninja
if exist "C:\msys64\mingw64\bin\ninja.exe" set GEN=Ninja

echo   Generator : !GEN!
echo   g++       : !GPP!
echo   CMake     : !CM!
echo.

"!CM!" -G "!GEN!" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ ..

if errorlevel 1 (
    echo.
    echo CMake configure FAILED. See errors above.
    cd ..
    pause
    exit /b 1
)

"!CM!" --build . --parallel

if errorlevel 1 (
    echo.
    echo Compile FAILED. See errors above.
    cd ..
    pause
    exit /b 1
)

cd ..

xcopy /E /I /Y python   build\python   >nul 2>&1
xcopy /E /I /Y analysis build\analysis >nul 2>&1
xcopy /E /I /Y scripts  build\scripts  >nul 2>&1

echo.
echo =============================================
echo  BUILD SUCCESSFUL
echo =============================================
echo.
echo  Executable: %CD%\build\ExtraGraphAnalyst.exe
echo.

set /p LAUNCH="  Launch now? [Y/N]: "
if /i "!LAUNCH!"=="Y" start "" "%CD%\build\ExtraGraphAnalyst.exe"

pause
endlocal
