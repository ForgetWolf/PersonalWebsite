$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    python build_static.py
    if ($LASTEXITCODE -ne 0) { throw 'Static build failed' }
    $compiler = Get-Command g++ -ErrorAction SilentlyContinue
    if ($compiler) { $compilerPath = $compiler.Source }
    else { $compilerPath = 'C:\Strawberry\c\bin\g++.exe' }
    & $compilerPath -std=c++17 -O2 -Wall -Wextra -static server.cpp -o server.exe -lws2_32
    if ($LASTEXITCODE -ne 0) { throw 'Static server compilation failed; stop the old server before rebuilding' }
    .\server.exe
} finally { Pop-Location }
