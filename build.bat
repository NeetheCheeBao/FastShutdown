@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

if not defined CXX set "CXX=g++"
if not defined WR  set "WR=windres"

where "%CXX%" >nul 2>&1
if errorlevel 1 (
  for %%D in ("C:\MinGW64\bin" "C:\mingw64\bin" "C:\msys64\mingw64\bin" "C:\TDM-GCC-64\bin") do (
    if exist "%%~D\%CXX%.exe" set "PATH=%%~D;!PATH!"
  )
)

where "%CXX%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: g++ not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set CXX=...
  echo.
  pause
  exit /b 1
)

where "%WR%" >nul 2>&1
if errorlevel 1 (
  echo ERROR: windres not found on PATH.
  echo Install MinGW-w64 and add its bin folder to PATH, or set WR=...
  echo.
  pause
  exit /b 1
)

set "OUT=dist\FastShutdown.exe"
set "OBJDIR=%TEMP%\FastShutdown_build"
set "RES=%OBJDIR%\app_res.o"

set "SRCS=main.cpp"

set /a TOTAL=2
for %%F in (%SRCS%) do set /a TOTAL+=1
set /a STEP=0

if not exist dist mkdir dist
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
mkdir "%OBJDIR%" 2>nul

taskkill /F /IM FastShutdown.exe >nul 2>&1

call :progress "resources (windres)"
"%WR%" -I . app.rc -o "%RES%"
if errorlevel 1 goto :fail

set "OBJS="
for %%F in (%SRCS%) do (
  set "name=%%F"
  set "name=!name:\=_!"
  set "name=!name:.cpp=.o!"
  call :progress "compile %%F"
  "%CXX%" -std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -I . -c "%%F" -o "%OBJDIR%\!name!"
  if errorlevel 1 goto :fail
  set OBJS=!OBJS! "%OBJDIR%\!name!"
)

call :progress "link %OUT%"
"%CXX%" !OBJS! "%RES%" -o "%OUT%" ^
  -mwindows ^
  -static-libgcc -static-libstdc++ -static -s ^
  -lole32 -luuid -lshell32 -ladvapi32 -luser32
if errorlevel 1 goto :fail

echo.
echo BUILD OK  -^> %OUT%
rmdir /s /q "%OBJDIR%" >nul 2>&1
call :hold 3
exit /b 0

:fail
echo.
echo BUILD FAILED
if exist "%OBJDIR%" rmdir /s /q "%OBJDIR%" >nul 2>&1
echo.
pause
exit /b 1

:hold
echo.
"%SystemRoot%\System32\timeout.exe" /t %~1 /nobreak >nul 2>&1
goto :eof

:progress
set /a STEP+=1
set /a "pct=STEP*100/TOTAL"
set /a "filled=STEP*30/TOTAL"
if !filled! gtr 30 set "filled=30"
set "bar="
for /L %%i in (1,1,30) do (
  if %%i LEQ !filled! (set "bar=!bar!=") else (set "bar=!bar! ")
)
echo [!bar!] !pct!%%  %~1
goto :eof
