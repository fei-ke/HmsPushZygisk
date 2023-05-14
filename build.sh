#!/usr/bin/env bash

set -euo pipefail

ndk-build -C module || exit 1

mkdir -p magisk/zygisk

for arch in arm64-v8a armeabi-v7a x86 x86_64
do
    cp module/libs/$arch/*.so magisk/zygisk/$arch.so
done

pushd magisk

version=`grep '^version=' module.prop | cut -d= -f2`
name=`grep '^id=' module.prop | cut -d= -f2`

rm -rf ../build
mkdir -p ../build

zip -r9 ../build/hmspush-zygisk-$version.zip .
popd