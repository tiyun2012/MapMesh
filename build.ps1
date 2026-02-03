# powershell -ExecutionPolicy Bypass -File .\build.ps1

$env:DEVKIT_LOCATION = "E:\dev\MAYA\API\C++\Autodesk_Maya_2026_3_Update_DEVKIT_Windows\devkitBase"


$cm = 'C:\Program Files\CMake\bin\cmake.exe'

# & $cm -S "$env:DEVKIT_LOCATION/devkit/plug-ins/MapMesh" `
#       -B "$env:DEVKIT_LOCATION/build/MapMesh" `
#       -G "Visual Studio 17 2022" -A x64 -DMATCHMESH_USE_TBB=ON
# & $cm --build $env:DEVKIT_LOCATION/build/MatchMesh --config Release



& $cm -S $env:DEVKIT_LOCATION/devkit/plug-ins/MatchMesh -B $env:DEVKIT_LOCATION/build/MatchMesh -G "Visual Studio 17 2022" -A x64 -DMATCHMESH_USE_TBB=ON
& $cm --build $env:DEVKIT_LOCATION/build/MatchMesh --config Release