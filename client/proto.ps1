$path = "./node_modules/.bin/protoc-gen-ts_proto.cmd"
if ([System.Environment]::OSVersion.Platform -eq "Win32NT") {
    $path = ".\\node_modules\\.bin\\protoc-gen-ts_proto.cmd"
}

$out = "./src/proto"

if (Test-Path -Path $out) {
    Remove-Item -Recurse -Path $out
}
New-Item -ItemType Directory -Path $out | Out-Null

& ../vcpkg_installed/arm64-android/tools/protobuf/protoc `
    --plugin=protoc-gen-ts_proto=$path `
    --ts_proto_opt=oneof=unions-value --ts_proto_opt=outputJsonMethods=false `
    --ts_proto_out=./src/proto --proto_path=../protos `
    ../protos/*.proto
