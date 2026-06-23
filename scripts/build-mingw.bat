@echo off
setlocal

set "CXX=%~1"
if "%CXX%"=="" set "CXX=g++"

"%CXX%" -std=c++17 -O2 -Wall -Wextra -pedantic -I include ^
    src\main.cpp src\Classifier.cpp src\FrameSource.cpp ^
    -o LiveAISignLanguageClassification.exe

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Built LiveAISignLanguageClassification.exe
