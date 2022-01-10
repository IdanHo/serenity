#!/usr/bin/env -S bash ../.port_include.sh
port=natalie
version=6205c559e0a826daf9fa7fecc43ecfbae3ba056d
onigmo_version=0830382895a303a478de988b7ba7af0f0e86bb6c
files="https://github.com/seven1m/natalie/archive/${version}.tar.gz natalie-${version}.tar.gz 1873fb2285b713e31db7aa6dc0983c1dc3f863d244a111016212b06a4a221468
https://github.com/k-takata/Onigmo/archive/${onigmo_version}.tar.gz Onigmo-${onigmo_version}.tar.gz bfff3ca15281142c4116a02350e0cddd721a816f96b7477b45954bfe5d9da2b0"
auth_type=sha256
depends=("git" "libtool" "make" "gcc" "ruby")

build() {
    cp -r "Onigmo-${onigmo_version}/." "$workdir/ext/onigmo"
    run rake
}
