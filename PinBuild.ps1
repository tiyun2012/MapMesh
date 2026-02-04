# powershell -ExecutionPolicy Bypass -File .\PinBuild.ps1

$env:DEVKIT_LOCATION = "E:\dev\MAYA\API\C++\Autodesk_Maya_2026_3_Update_DEVKIT_Windows\devkitBase"

$cm = "C:\Program Files\CMake\bin\cmake.exe"

$src = "$env:DEVKIT_LOCATION/devkit/plug-ins/MatchMesh/pinObject"
$bld = "$env:DEVKIT_LOCATION/build/PinLocator"

& $cm -S $src -B $bld -G "Visual Studio 17 2022" -A x64
& $cm --build $bld --config Release
