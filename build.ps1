<#
.SYNOPSIS
    Збирає транслятор MiniGroovy (minigroovy.exe) і, за потреби, запускає тести.

.DESCRIPTION
    Скрипт компілює всі файли src/**/*.cpp компілятором g++ (MinGW-w64).
    Виконуваний файл лінкується статично: стандартна бібліотека C++ і
    runtime GCC вбудовуються в exe, тому він запускається на будь-якій
    64-бітній Windows без встановленого MinGW і без додаткових DLL.

.PARAMETER Configuration
    Release (за замовчуванням): оптимізація -O2, без налагоджувальної інформації.
    Debug: -O0 -g для налагодження.

.PARAMETER Test
    Також зібрати minigroovy_tests.exe і запустити модульні тести
    та перевірки на прикладах з каталогу examples/.

.PARAMETER Clean
    Видалити каталог build/ перед збіркою.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Test
    .\build.ps1 -Configuration Debug -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = $PSScriptRoot
$srcDir = Join-Path $root 'src'
$testsDir = Join-Path $root 'tests'
$examplesDir = Join-Path $root 'examples'
$buildDir = Join-Path $root 'build'

# Пошук g++: спершу в PATH, потім у типових каталогах встановлення MinGW.
function Find-Compiler {
    $command = Get-Command 'g++' -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        'C:\Program Files (x86)\mingw64\bin\g++.exe',
        'C:\Program Files\mingw64\bin\g++.exe',
        'C:\mingw64\bin\g++.exe',
        'C:\msys64\ucrt64\bin\g++.exe',
        'C:\msys64\mingw64\bin\g++.exe'
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }
    throw 'g++ was not found. Install MinGW-w64 and add its bin directory to PATH.'
}

# Компіляція й лінкування одним викликом g++ (проєкт невеликий).
function Invoke-Compiler([string]$compiler, [string[]]$sources, [string]$output, [string[]]$includeDirs) {
    $flags = @('-std=c++20', '-Wall', '-Wextra', '-Wpedantic', '-Werror')
    if ($Configuration -eq 'Release') {
        $flags += @('-O2', '-s')
    } else {
        $flags += @('-O0', '-g')
    }
    # Статичне лінкування: libstdc++, libgcc і winpthread потрапляють усередину exe.
    $flags += @('-static', '-static-libgcc', '-static-libstdc++')
    foreach ($dir in $includeDirs) { $flags += "-I$dir" }

    Write-Host "Building $(Split-Path $output -Leaf) ($Configuration)..."
    & $compiler @flags @sources -o $output
    if ($LASTEXITCODE -ne 0) { throw "Compilation of $(Split-Path $output -Leaf) failed." }
}

# Перевірка, що exe не залежить від DLL MinGW, яких може не бути на іншій машині.
function Test-NoMinGwDependencies([string]$compiler, [string]$exe) {
    $objdump = Join-Path (Split-Path $compiler) 'objdump.exe'
    if (-not (Test-Path $objdump)) {
        Write-Warning 'objdump.exe was not found; DLL dependency check skipped.'
        return
    }
    $dlls = & $objdump -p $exe | Select-String 'DLL Name:\s*(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
    Write-Host "DLL dependencies: $($dlls -join ', ')"

    $forbidden = $dlls | Where-Object { $_ -match '^(libstdc\+\+|libgcc|libwinpthread|libssp|libatomic|libgomp|libquadmath)' }
    if ($forbidden) {
        throw "Executable depends on MinGW runtime DLLs: $($forbidden -join ', ')"
    }
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$compiler = Find-Compiler
Write-Host "Compiler: $compiler"

$appSources = @(Get-ChildItem -Path $srcDir -Recurse -Filter '*.cpp' | ForEach-Object { $_.FullName })
$appExe = Join-Path $buildDir 'minigroovy.exe'
Invoke-Compiler $compiler $appSources $appExe @($srcDir)
Test-NoMinGwDependencies $compiler $appExe
Write-Host "Done: $appExe" -ForegroundColor Green

if ($Test) {
    # Тести використовують увесь код лексера, крім main.cpp транслятора.
    $libSources = @($appSources | Where-Object { (Split-Path $_ -Leaf) -ne 'main.cpp' })
    $testSources = @(Get-ChildItem -Path $testsDir -Filter '*.cpp' | ForEach-Object { $_.FullName })
    $testExe = Join-Path $buildDir 'minigroovy_tests.exe'
    Invoke-Compiler $compiler ($libSources + $testSources) $testExe @($srcDir, $testsDir)

    Write-Host 'Running tests...'
    & $testExe $examplesDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Tests FAILED' -ForegroundColor Red
        exit 1
    }
    Write-Host 'All tests passed' -ForegroundColor Green
}
