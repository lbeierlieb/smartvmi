{
  # deps
  bison
, cmake
, flex
, fuse
, glib
, json_c
, libkvmi
, libvirt
, pkg-config
, xen
, # helpers
  lib
, stdenv
, # source files
  source
}:
stdenv.mkDerivation rec {
  name = "libvmi-gdata";
  version = "0.15.0-gdata";
  libVersion = "0.0.15";
  src = source;
  nativeBuildInputs = [
    bison
    cmake
    flex
    pkg-config
  ];
  buildInputs = [
    fuse
    glib
    json_c
    libkvmi
    libvirt
    xen
  ];
  cmakeFlags = [
    "-D ENABLE_STATIC=OFF"
    "-D BUILD_EXAMPLES=OFF"
  ];
  postFixup = ''
    libvmi="$out/lib/libvmi.so.${libVersion}"
    oldrpath=$(patchelf --print-rpath "$libvmi")
    patchelf --set-rpath "$oldrpath:${lib.makeLibraryPath [ xen libvirt libkvmi ]}" "$libvmi"
  '';
  postInstall = ''
    mkdir -p $out/lib/cmake/libvmi
    cp ${./libvmi-config.cmake} $out/lib/cmake/libvmi/libvmiConfig.cmake
  '';
}
