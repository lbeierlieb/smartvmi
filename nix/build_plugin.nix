{
  # plugin parameters
  name
, additionalDeps ? [ ]
, # deps
  cmake
, fmt
, jsoncpp
, pkg-config
, tclap
, yaml-cpp
, # helpers
  nix-gitignore
, stdenv
}:
let
  pluginSrc = nix-gitignore.gitignoreSource [ ] ../plugins/${name};
  vmicoreSrc = nix-gitignore.gitignoreSource [ ] ../vmicore;
in
stdenv.mkDerivation {
  name = "smartvmi-plugin-${name}";
  src = pluginSrc;

  cmakeFlags = [
    "-D VMICORE_DIRECTORY_ROOT=${vmicoreSrc}"
  ];

  buildInputs = [
    fmt.dev
    jsoncpp.dev
    tclap
    yaml-cpp
  ]
  ++ additionalDeps;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];
}
