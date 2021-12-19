#!/usr/bin/env -S bash ../.port_include.sh

port="wine"
major_version="6"
version="${major_version}.23"
useconfigure=true
auth_type=sha256
use_fresh_config_sub=true
config_sub_paths=("tools/config.sub")
files="https://dl.winehq.org/wine/source/${major_version}.x/wine-${version}.tar.xz wine-${version}.tar.xz d96c7f31741e4251fb50f919309c604665267cfacee13f8f450a30c953620e0d"
depends=("SDL2" "libtiff" "libpng" "libjpeg" "freetype")
configopts=("--without-alsa" "--without-capi" "--without-cms" "--without-coreaudio" "--without-cups" "--without-dbus" "--without-faudio" "--without-fontconfig" "--without-gettext" "--without-gphoto" "--without-gnutls" "--without-gsm" "--without-gssapi" "--without-gstreamer" "--without-hal" "--without-inotify" "--without-krb5" "--without-ldap" "--without-mingw" "--without-mpg123" "--without-netapi" "--without-openal" "--without-opencl" "--without-osmesa" "--without-oss" "--without-pcap" "--without-pulse" "--without-quicktime" "--without-sane" "--without-udev" "--without-usb" "--without-v4l2" "--without-vkd3d" "--without-vulkan" "--without-x" "--without-xcomposite" "--without-xcursor" "--without-xfixes" "--without-xinerama" "--without-xinput" "--without-xinput2" "--without-xml" "--without-xrandr" "--without-xrender" "--without-xshape" "--without-xshm" "--without-xslt" "--without-xxf86vm")

# Note: The showproperty command is used when linting ports, we don't actually need wine tools at this time.
if [ "$1" != "showproperty" ]; then
	if [ -z ${WINE_TOOLS+x} ]; then
		echo "Error: Host-compiled wine tools are required for cross-compiling wine" >&2
		echo "Build wine ${version} on your host (__tooldeps__ make target) and specify the build root with \$WINE_TOOLS" >&2
		exit 1
	else
		configopts+=("--with-wine-tools=${WINE_TOOLS}")
	fi
fi

function pre_configure() {
	export SDL2_CFLAGS="-I${SERENITY_INSTALL_ROOT}/usr/local/include/SDL2"
	export FREETYPE_CFLAGS="-I${SERENITY_INSTALL_ROOT}/usr/local/include/freetype2/"
	export TIFF_CFLAGS="-I${SERENITY_INSTALL_ROOT}/usr/local/include/"
	export PNG_CFLAGS="-I${SERENITY_INSTALL_ROOT}/usr/local/include/libpng16/"
}

function post_configure() {
	unset SDL2_CFLAGS
	unset FREETYPE_CFLAGS
	unset TIFF_CFLAGS
	unset PNG_CFLAG
}
