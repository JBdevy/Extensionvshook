@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
title Subir VSHookExt e disparar builds

cd /d "%~dp0"
if errorlevel 1 goto erro_pasta

cls
echo ==============================================
echo   SUBIR VSHookExt + DISPARAR WINDOWS E MACOS
echo ==============================================
echo.
echo Este processo atualiza a versao no CMakeLists.txt,
echo cria o commit, envia a branch e recria a tag vX.Y.Z.
echo A tag dispara os dois workflows do GitHub Actions.
echo.

where git >nul 2>&1
if errorlevel 1 goto erro_git

where powershell >nul 2>&1
if errorlevel 1 goto erro_powershell

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 goto erro_repositorio

for /f "delims=" %%B in ('git branch --show-current') do set "BRANCH=%%B"
if not defined BRANCH goto erro_branch

set "CURRENT_VERSION="
for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "$raw=Get-Content -Raw -LiteralPath 'CMakeLists.txt'; if($raw -match 'project\(reaper_vshook_loader VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)'){$matches[1]}"`) do set "CURRENT_VERSION=%%V"
if not defined CURRENT_VERSION set "CURRENT_VERSION=1.0.0"

:pedir_versao
set "VERSION="
set /p "VERSION=Versao da VSHookExt [%CURRENT_VERSION%]: "
if not defined VERSION set "VERSION=%CURRENT_VERSION%"
if /i "%VERSION:~0,1%"=="v" set "VERSION=%VERSION:~1%"

echo(%VERSION%| findstr /r /x "[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*" >nul
if errorlevel 1 goto versao_invalida

set "TAG_NAME=v%VERSION%"
set "DEFAULT_MSG=VSHookExt %VERSION%"
set "COMMIT_MSG="
set /p "COMMIT_MSG=Mensagem do commit [%DEFAULT_MSG%]: "
if not defined COMMIT_MSG set "COMMIT_MSG=%DEFAULT_MSG%"

echo.
echo Branch:  %BRANCH%
echo Versao:  %VERSION%
echo Tag:     %TAG_NAME%
echo Commit:  %COMMIT_MSG%
echo Saidas:  reaper_VSHookExt.dll e reaper_VSHookExt.dylib
echo.
choice /c SN /n /m "Continuar? [S/N]: "
if errorlevel 2 goto cancelado

echo.
echo Atualizando a versao no CMakeLists.txt...
powershell -NoProfile -Command "$path='CMakeLists.txt'; $raw=Get-Content -Raw -LiteralPath $path; $next=[regex]::Replace($raw,'project\(reaper_vshook_loader VERSION [0-9]+\.[0-9]+\.[0-9]+ LANGUAGES CXX\)','project(reaper_vshook_loader VERSION %VERSION% LANGUAGES CXX)',1); if($next -eq $raw -and $raw -notmatch 'VERSION %VERSION%'){throw 'Linha de versao nao encontrada.'}; [IO.File]::WriteAllText((Resolve-Path $path),$next,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 goto erro

echo.
echo Adicionando arquivos...
git add -A
if errorlevel 1 goto erro

git diff --cached --quiet
if not errorlevel 1 goto sem_alteracoes

echo.
echo Criando commit...
git commit -m "%COMMIT_MSG%"
if errorlevel 1 goto erro

:sem_alteracoes
echo.
echo Enviando branch %BRANCH%...
git push origin "%BRANCH%"
if errorlevel 1 goto erro

git show-ref --verify --quiet "refs/tags/%TAG_NAME%"
if errorlevel 1 goto verificar_tag_remota
echo Removendo tag local existente %TAG_NAME%...
git tag -d "%TAG_NAME%"
if errorlevel 1 goto erro

:verificar_tag_remota
git ls-remote --exit-code --tags origin "refs/tags/%TAG_NAME%" >nul 2>&1
set "REMOTE_TAG_CHECK=%ERRORLEVEL%"
if "%REMOTE_TAG_CHECK%"=="0" goto excluir_tag_remota
if "%REMOTE_TAG_CHECK%"=="2" goto criar_tag
goto erro_consulta_tag

:excluir_tag_remota
echo Removendo tag remota existente %TAG_NAME%...
git push origin --delete "%TAG_NAME%"
if errorlevel 1 goto erro

:criar_tag
echo Criando tag %TAG_NAME%...
git tag -a "%TAG_NAME%" -m "%COMMIT_MSG%"
if errorlevel 1 goto erro

echo Enviando tag e disparando os dois workflows...
git push origin "%TAG_NAME%"
if errorlevel 1 goto erro

echo.
echo ==============================================
echo   PRONTO
echo ==============================================
echo Tag enviada: %TAG_NAME%
echo Builds disparadas: Windows x64 e macOS Universal.
echo.
pause
exit /b 0

:versao_invalida
echo.
echo ERRO: use o formato X.Y.Z, por exemplo 1.0.1.
echo.
goto pedir_versao

:erro_consulta_tag
echo ERRO: nao foi possivel consultar a tag no GitHub.
goto erro

:cancelado
echo.
echo Operacao cancelada.
pause
exit /b 0

:erro_pasta
echo ERRO: nao foi possivel abrir a pasta da extensao.
pause
exit /b 1

:erro_git
echo ERRO: Git nao foi encontrado no PATH.
pause
exit /b 1

:erro_powershell
echo ERRO: PowerShell nao foi encontrado no PATH.
pause
exit /b 1

:erro_repositorio
echo ERRO: esta pasta ainda nao e um repositorio Git valido.
echo Inicialize ou clone o repositorio antes de usar este BAT.
pause
exit /b 1

:erro_branch
echo ERRO: nao consegui detectar a branch atual.
pause
exit /b 1

:erro
echo.
echo ==============================================
echo   A OPERACAO NAO FOI CONCLUIDA
echo ==============================================
echo Verifique a mensagem acima.
echo.
pause
exit /b 1
