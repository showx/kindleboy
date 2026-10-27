#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# build.ps1 prepares the pinned dependencies and demo with host Python first.
mkdir -p downloads/toolchain
TC=/work/downloads/toolchain/x-tools/arm-kindlepw2-linux-gnueabi/bin/arm-kindlepw2-linux-gnueabi
if [ ! -f "$TC-gcc" ]; then
    tar xf downloads/kindlepw2.tar.gz -C downloads/toolchain
fi
make headless sim statetest
# Relink the sanitizer test to the builder image's runtime libraries.
make -B uitest
# Some Docker Desktop kernels recurse in ASan's SIGSEGV handler. Keep address
# instrumentation/abort-on-error, and let the kernel handle fatal signals.
ASAN_OPTIONS=handle_segv=0:abort_on_error=1:detect_leaks=0 ./build/uitest
(
    cd build/demo
    ../headless KindleBoy-Demo.gb 120 120
    ../statetest KindleBoy-Demo.gb
    ../simrun play /work/build/demo/KindleBoy-Demo.gb
    ../simrun browser /work/build/demo
    ../simrun feedback /work/build/demo/KindleBoy-Demo.gb
)
sh -n kual/kindleboy/bin/run.sh
sh test/launcher.sh
sh test/boot.sh
make kindle CROSS="$TC"
file build/kindleboy
