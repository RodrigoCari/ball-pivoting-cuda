@echo off
REM Integrantes: IVAN ALEXANDER LOPEZ ZEGARRA | JESUS BRUNO CHANCAYAURI SONCCO | JOSE RODRIGO CARI ALMIRON
REM Curso: Computación Gráfica - Laboratorio Ball Pivoting
setlocal EnableExtensions
chcp 65001 >nul
cd /d "%~dp0"
where wsl.exe >nul 2>nul
if errorlevel 1 (
  echo No se encontro WSL. Instala Ubuntu WSL2 e intentalo nuevamente.
  exit /b 1
)
for /f "usebackq delims=" %%D in (`wsl.exe -e wslpath -a "%CD%"`) do set "LINUX_DIR=%%D"
if not defined LINUX_DIR (
  echo No se pudo determinar la ruta del proyecto en WSL.
  exit /b 1
)
wsl.exe --cd "%LINUX_DIR%" -e bash ./ejecutar_pruebas.sh %*
exit /b %ERRORLEVEL%
