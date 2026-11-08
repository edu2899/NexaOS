@echo off
cd /d "%~dp0" || exit /b 1
set "MODULOS=src\db.c src\util.c src\clientes.c src\equipamentos.c"

if /i "%~1"=="test" (
    if not exist build mkdir build || exit /b 1
    for %%F in (tests\test_*.c) do (
        gcc -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude "%%F" %MODULOS% -o "build\%%~nF.exe" -lsqlite3 || exit /b 1
        "build\%%~nF.exe" || exit /b 1
    )
    echo TESTES OK
    exit /b 0
)

if not exist build mkdir build || exit /b 1
gcc -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude src\main.c %MODULOS% -o build\nexaos.exe -lsqlite3 || exit /b 1

if /i "%~1"=="run" build\nexaos.exe || exit /b 1
