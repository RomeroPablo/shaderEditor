<#
    Windows companion to ./build.
    Compiles artifacts\shaders.exe with clang++/g++ and launches it from artifacts/.
    Pass -Clean to remove artifacts or -SkipRun to skip launching the app.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$SkipRun
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot
if (-not $repoRoot) {
    $repoRoot = Get-Location
}

Push-Location $repoRoot
try {

    $artifactsDir = Join-Path $repoRoot 'artifacts'
    $binaryName = 'shaders.exe'
    $binaryPath = Join-Path $artifactsDir $binaryName

    if ($Clean) {
        if (Test-Path $artifactsDir) {
            Remove-Item $artifactsDir -Recurse -Force
            Write-Host "Removed $artifactsDir"
        } else {
            Write-Host 'Nothing to clean.'
        }
        return
    }

    $sourceFiles = @('main.cpp', 'helpers.hpp')
    $needsBuild = -not (Test-Path $binaryPath)
    if (-not $needsBuild) {
        $binaryStamp = (Get-Item $binaryPath).LastWriteTimeUtc
        foreach ($src in $sourceFiles) {
            $srcPath = Join-Path $repoRoot $src
            if ((Get-Item $srcPath).LastWriteTimeUtc -gt $binaryStamp) {
                $needsBuild = $true
                break
            }
        }
    }

    if ($needsBuild) {
        if (-not (Test-Path $artifactsDir)) {
            New-Item $artifactsDir -ItemType Directory -Force | Out-Null
        }

        $compiler = $env:CXX
        if (-not $compiler) {
            foreach ($candidate in @('clang++', 'g++')) {
                $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
                if ($cmd) {
                    $compiler = $cmd.Source
                    break
                }
            }
        }
        if (-not $compiler) {
            throw 'Unable to locate clang++ or g++. Install a compiler or set CXX.'
        }

        $sdkRoot = if ($env:VULKAN_SDK) { $env:VULKAN_SDK } else { 'C:\VulkanSDK\1.4.341.1' }
        if (-not (Test-Path $sdkRoot)) {
            throw "Vulkan SDK directory '$sdkRoot' was not found. Set VULKAN_SDK if it is installed elsewhere."
        }

        $includeDirs = @()
        $includeDirs += (Join-Path $sdkRoot 'Include') # user noted this holds both Vulkan + SDK headers
        if ($env:SDL2_DIR) {
            $sdlInclude = Join-Path $env:SDL2_DIR 'include'
            if (Test-Path $sdlInclude) {
                $includeDirs += $sdlInclude
            }
        }

        $libDirs = @()
        $defaultLib = Join-Path $sdkRoot 'Lib'
        if (Test-Path $defaultLib) {
            $libDirs += $defaultLib
        }
        if ($env:SDL2_DIR) {
            $sdlLibCandidates = @(
                (Join-Path $env:SDL2_DIR 'lib'),
                (Join-Path $env:SDL2_DIR 'lib\x64'),
                (Join-Path $env:SDL2_DIR 'lib\win64')
            )
            foreach ($candidate in $sdlLibCandidates) {
                if (Test-Path $candidate) {
                    $libDirs += $candidate
                }
            }
        }

        $includeDirs = $includeDirs | Where-Object { $_ } | Select-Object -Unique
        $libDirs = $libDirs | Where-Object { $_ } | Select-Object -Unique

        $compileArgs = @('-std=c++23', '-O3', 'main.cpp')
        foreach ($inc in $includeDirs) {
            $compileArgs += @('-I', $inc)
        }
        foreach ($libDir in $libDirs) {
            $compileArgs += @('-L', $libDir)
        }
        $compileArgs += @('-o', $binaryPath, '-lSDL2', '-lSDL2main', '-lvulkan-1')

        Write-Host "$compiler $($compileArgs -join ' ')"
        & $compiler @compileArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE"
        }
    } else {
        Write-Host 'artifacts\shaders.exe is up to date.'
    }

    if ($SkipRun) {
        return
    }

    Push-Location $artifactsDir
    try {
        & ".\${binaryName}"
    } finally {
        Pop-Location
    }
}
finally {
    Pop-Location
}
