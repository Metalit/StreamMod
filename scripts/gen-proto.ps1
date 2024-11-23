& vcpkg install --triplet arm64-android
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (Test-Path ./protos_cpp) {
    Remove-Item ./protos_cpp -Recurse -Force
}

New-Item ./protos_cpp -ItemType Directory | Out-Null

& $PSScriptRoot/../vcpkg_installed/arm64-android/tools/protobuf/protoc -Iprotos --cpp_out=./protos_cpp ./protos/*.proto
