@echo off
setlocal

echo === voxforma Windows-Build ===
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo [FEHLER] cmake nicht gefunden. Bitte CMake installieren und in PATH aufnehmen.
    exit /b 1
)

if not exist "%~dp0JUCE\CMakeLists.txt" (
    echo [FEHLER] JUCE-Submodul nicht gefunden unter %~dp0JUCE
    echo         Bitte zuerst ausfuehren:
    echo             git submodule update --init --recursive
    echo             patches\apply-juce-patches.sh
    exit /b 1
)

if not exist "%~dp0build-win" mkdir "%~dp0build-win"

cmake -S "%~dp0" -B "%~dp0build-win" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo [HINWEIS] Visual Studio 2022 nicht gefunden, versuche Ninja...
    cmake -S "%~dp0" -B "%~dp0build-win" -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 exit /b 1
)

cmake --build "%~dp0build-win" --config Release --parallel
if errorlevel 1 exit /b 1

echo.
echo === Fertig ===
echo Artefakt: %~dp0build-win\voxforma_artefacts\Release\VST3\voxforma.vst3
endlocal
