@echo off
setlocal

set BIN=bin
if not exist %BIN% mkdir %BIN%

rem === AVX2 ===
echo ▶ Compilo AVX2...
mingw32-make clean >nul 2>&1
mingw32-make profile-build ARCH=x86-64-avx2 COMP=mingw -j 12
if exist hypnos.exe (
    move /Y hypnos.exe %BIN%\hypnos-avx2.exe >nul
    echo ✔ hypnos-avx2.exe creato
) else (
    echo ✘ Errore build AVX2
)

rem === BMI2 ===
echo ▶ Compilo BMI2...
mingw32-make clean >nul 2>&1
mingw32-make profile-build ARCH=x86-64-bmi2 COMP=mingw -j 12
if exist hypnos.exe (
    move /Y hypnos.exe %BIN%\hypnos-bmi2.exe >nul
    echo ✔ hypnos-bmi2.exe creato
) else (
    echo ✘ Errore build BMI2
)

rem === SSE41 + POPCNT ===
echo ▶ Compilo SSE41-POPCNT...
mingw32-make clean >nul 2>&1
mingw32-make profile-build ARCH=x86-64-sse41-popcnt COMP=mingw -j 12
if exist hypnos.exe (
    move /Y hypnos.exe %BIN%\hypnos-sse41.exe >nul
    echo ✔ hypnos-sse41.exe creato
) else (
    echo ✘ Errore build SSE41
)

rem === GENERIC ===
echo ▶ Compilo GENERIC...
mingw32-make clean >nul 2>&1
mingw32-make profile-build ARCH=x86-64 COMP=mingw -j 12
if exist hypnos.exe (
    move /Y hypnos.exe %BIN%\hypnos-generic.exe >nul
    echo ✔ hypnos-generic.exe creato
) else (
    echo ✘ Errore build GENERIC
)

echo.
echo Tutte le build completate. Gli eseguibili si trovano in %BIN%\
echo.
pause
endlocal
