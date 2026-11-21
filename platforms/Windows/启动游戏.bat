@echo off
chcp 65001 >nul
cd /d "%~dp0"
if /i "%~1"=="--java" goto java
if exist "%~dp0Native\byow_native.exe" (
    start "" /d "%~dp0Native" "%~dp0Native\byow_native.exe"
    exit /b
)

:java
if exist BYOW.exe (
    start "" "%~dp0BYOW.exe"
) else (
    cd /d "%~dp0..\.."
    where py >nul 2>nul
    if errorlevel 1 (
        python run.py
    ) else (
        py -3 run.py
    )
    if errorlevel 1 pause
)
