@echo off
REM ============================================================
REM Batch script per compilare l'engine ONScripter su Windows
REM utilizzando MSYS2 (shell mingw32) tramite uno script temporaneo
REM ============================================================

REM Imposta il percorso di installazione di MSYS2 (modifica se necessario)
set "MSYS2_ROOT=A:\MSYS2"

REM Imposta la directory del progetto in stile Linux (per MSYS2)
set "PROJECT_DIR=/b/Umineko-Project-Scripting/onscripter-ru"

REM Verifica se MSYS2 è installato
if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo MSYS2 non è stato trovato in %MSYS2_ROOT%.
    pause
    exit /b 1
)

REM Crea uno script Bash temporaneo nella cartella TEMP
set "TEMP_SCRIPT=%TEMP%\build_onscripter.sh"
(
    echo cd %PROJECT_DIR%
    echo echo Running configure...
    echo ./configure
    echo echo Running make...
    echo make
    echo echo Build complete.
    echo echo Press any key to exit...
    echo read -n 1 -s -r
) > "%TEMP_SCRIPT%"

REM Avvia la shell MSYS2 e esegue lo script temporaneo
"%MSYS2_ROOT%\msys2_shell.cmd" -mingw32 -use-full-path -here -c "bash \"%TEMP_SCRIPT%\""

REM Elimina lo script temporaneo
del "%TEMP_SCRIPT%"

pause
