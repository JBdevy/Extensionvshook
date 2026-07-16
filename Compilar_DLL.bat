@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Compilar VS Hook DLL

set "ROOT=%~dp0"
set "EXT=%ROOT%extension"
set "OUT=%ROOT%reaper_vshook.dll"
set "BUILD=%EXT%\build-win-local"

where cl.exe >nul 2>nul
if errorlevel 1 (
  echo Inicializando ambiente do Visual Studio...
  call :LOAD_VS_ENV
  if errorlevel 1 goto :FAIL
)

where cmake.exe >nul 2>nul
if errorlevel 1 (
  echo ERRO: cmake.exe nao encontrado.
  echo Instale o componente CMake tools for Windows no Visual Studio.
  pause
  exit /b 1
)

if not exist "%EXT%\CMakeLists.txt" (
  echo ERRO: pasta extension nao encontrada ao lado deste BAT.
  pause
  exit /b 1
)

if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%" >nul 2>nul

where ninja.exe >nul 2>nul
if errorlevel 1 (
  set "CMAKE_GENERATOR=NMake Makefiles"
) else (
  set "CMAKE_GENERATOR=Ninja"
)

echo Usando gerador CMake: !CMAKE_GENERATOR!
cmake -S "%EXT%" -B "%BUILD%" -G "!CMAKE_GENERATOR!" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :FAIL

cmake --build "%BUILD%" --config Release
if errorlevel 1 goto :FAIL

if exist "%BUILD%\reaper_vshook.dll" (
  copy /y "%BUILD%\reaper_vshook.dll" "%OUT%" >nul
  echo.
  echo OK: DLL criada em:
  echo %OUT%
  pause
  exit /b 0
)

for /r "%BUILD%" %%F in (reaper_vshook.dll) do (
  copy /y "%%F" "%OUT%" >nul
  echo.
  echo OK: DLL criada em:
  echo %OUT%
  pause
  exit /b 0
)

echo ERRO: compilou mas nao achei reaper_vshook.dll.
pause
exit /b 1

:LOAD_VS_ENV
set "VCVARS="
for %%P in (
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do (
  if exist %%~P set "VCVARS=%%~P"
)

if not defined VCVARS (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
    if defined VSINSTALL if exist "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
  )
)

if not defined VCVARS (
  echo ERRO: vcvars64.bat nao encontrado.
  echo Instale "Desenvolvimento para desktop com C++" no Visual Studio Installer.
  exit /b 1
)

call "%VCVARS%"
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo ERRO: cl.exe nao encontrado depois do vcvars64.
  exit /b 1
)
exit /b 0

:FAIL
echo.
echo ERRO NA COMPILACAO.
pause
exit /b 1
