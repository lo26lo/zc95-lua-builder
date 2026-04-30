@echo off
REM ============================================================
REM  Deploy a portable build of zc95-lua-builder
REM  Output: dist\zc95-lua-builder\  (self-contained, zippable)
REM ============================================================

setlocal
set ROOT=%~dp0
set BUILD=%ROOT%build
set OUT=%ROOT%dist\zc95-lua-builder
set QTBIN=C:\Qt\6.8.3\msvc2022_64\bin
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat

REM Active l'environnement MSVC pour que windeployqt --compiler-runtime
REM puisse trouver vcruntime140.dll / msvcp140.dll
if exist "%VCVARS%" (
    call "%VCVARS%" x64 >nul
) else (
    echo [WARN] vcvarsall.bat introuvable, les DLL MSVC ne seront pas copiees.
)

if not exist "%BUILD%\zc95-lua-builder.exe" (
    echo [ERROR] %BUILD%\zc95-lua-builder.exe introuvable.
    echo         Lance d'abord _build.bat pour compiler.
    exit /b 1
)

if not exist "%QTBIN%\windeployqt.exe" (
    echo [ERROR] windeployqt.exe introuvable dans %QTBIN%
    exit /b 1
)

echo [1/3] Preparation du dossier portable...
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%"

echo [2/3] Copie de l'executable...
copy /y "%BUILD%\zc95-lua-builder.exe" "%OUT%\" >nul
if errorlevel 1 exit /b 1

echo [3/4] Deploiement des DLL Qt via windeployqt...
"%QTBIN%\windeployqt.exe" ^
    --release ^
    --no-translations ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-quick-import ^
    --compiler-runtime ^
    "%OUT%\zc95-lua-builder.exe"
if errorlevel 1 exit /b 1

echo [4/4] Copie des DLL runtime MSVC (vcruntime140, msvcp140...)...
set CRT=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT
if exist "%CRT%" (
    copy /y "%CRT%\vcruntime140.dll"   "%OUT%\" >nul
    copy /y "%CRT%\vcruntime140_1.dll" "%OUT%\" >nul
    copy /y "%CRT%\msvcp140.dll"       "%OUT%\" >nul
    copy /y "%CRT%\msvcp140_1.dll"     "%OUT%\" >nul
    copy /y "%CRT%\msvcp140_2.dll"     "%OUT%\" >nul
    REM On supprime l'installeur vc_redist devenu inutile
    if exist "%OUT%\vc_redist.x64.exe" del "%OUT%\vc_redist.x64.exe"
) else (
    echo [WARN] Dossier CRT introuvable, vc_redist.x64.exe est conserve.
    echo        L'utilisateur final devra l'executer une fois.
)

echo.
echo [OK] Build portable disponible dans :
echo      %OUT%
echo.
echo Vous pouvez zipper ce dossier et l'utiliser sur n'importe quelle
echo machine Windows 64 bits sans installer Qt.
endlocal
