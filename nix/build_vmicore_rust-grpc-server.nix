{
  # deps
  craneLib
, protobuf
, # helpers
  nix-gitignore
}:

let
  commonArgs = {
    src = nix-gitignore.gitignoreSource [ ] ../vmicore/rust_src;
    nativeBuildInputs = [ protobuf ];
  };

  # Downloaded and compiled dependencies.
  cargoArtifacts = craneLib.buildDepsOnly commonArgs;

  cargoPackage = craneLib.buildPackage (commonArgs // { inherit cargoArtifacts; });

  cargoVendored = craneLib.vendorCargoDeps commonArgs;
in
{
  inherit cargoPackage;
  inherit cargoVendored;
}
