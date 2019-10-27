#!/usr/bin/env bash

set -euf -o pipefail

cat <<EOF > /tmp/cross
[binaries]
c = '/usr/bin/arm-linux-gnueabihf-gcc'
cpp = '/usr/bin/arm-linux-gnueabihf-g++'
ar = '/usr/bin/arm-linux-gnueabihf-ar'
strip = '/usr/bin/arm-linux-gnueabihf-strip'
pkgconfig = '/usr/bin/arm-linux-gnueabihf-pkg-config'

[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7hl'
endian = 'little'
EOF

meson build \
	-Ddri-drivers= \
	-Dplatforms=x11,drm,surfaceless \
	--cross-file=/tmp/cross \
	-DI-love-half-baked-turnips=true \
	-Dvulkan-drivers=broadcom,freedreno \
	-Dgallium-drivers=v3d \
	--prefix=/usr \
	--libdir=lib/arm-linux-gnueabihf \
	-Dlibunwind=false \
	-Dllvm=false \
