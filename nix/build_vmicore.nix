{
  # deps
  bext-di
, bison
, boost
, cmake
, corrosion
, flex
, fuse
, fmt
, glib
, json_c
, libkvmi
, libvirt
, libvmi
, microsoft-gsl
, pkg-config
, rustc
, tclap
, yaml-cpp
, zlib
, # indirect deps
  craneLib
, # helpers
  callPackage
, nix-gitignore
, stdenv
}:

let
  libRustGrpcServer = callPackage ./build_vmicore_rust-grpc-server.nix { inherit craneLib; };
  cxxbridge-cmd = callPackage ./build_cxxbridge-cmd.nix { rustLib = libRustGrpcServer; };
in
stdenv.mkDerivation {
  name = "vmicore";
  src = nix-gitignore.gitignoreSource [ ] ../vmicore;

  # Prevent any online access.
  CARGO_NET_OFFLINE = "true";

  # Prepare the environment for Cargo so that no network access is required.
  preConfigure = ''
    mkdir -p rust_src/.cargo
    cat ${libRustGrpcServer.cargoVendored}/config.toml >> rust_src/.cargo/config.toml
  '';

  cmakeFlags = [
    # For debugging CMake Build:
    # "-Wdev"
    # "--debug-output"
    # "--trace"
    # "-D CMAKE_EXECUTE_PROCESS_COMMAND_ECHO=STDOUT"

    # Build settings
    "-D CMAKE_BUILD_TYPE=Release"
  ];
  nativeBuildInputs =
    [
      bison
      cmake
      corrosion
      cxxbridge-cmd # for corrosion
      flex
      pkg-config
      rustc
    ]
    ++ libRustGrpcServer.cargoPackage.nativeBuildInputs;

  buildInputs = [
    bext-di
    boost
    fmt.dev
    fuse
    glib
    json_c
    libkvmi
    libvirt
    libvmi
    microsoft-gsl
    tclap
    yaml-cpp
    zlib
  ];
}
