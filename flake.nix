{
  description = "smartvmi";

  inputs = {
    crane.url = "github:ipetkov/crane/master";
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    # source for boost-ext DI. When updating URL to a newer version here,
    # remember to also update the version attribute in the bext-di derivation.
    bext-di = {
      url = "github:boost-ext/di?ref=v1.3.2";
      flake = false;
    };
  };

  outputs =
    { self, flake-parts, ... }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      flake = { };
      systems = [ "x86_64-linux" ];
      perSystem =
        { system
        , self'
        , pkgs
        , ...
        }:
        let
          bext-di = pkgs.callPackage ./nix/build_bext-di.nix { source = inputs.bext-di; };
        in
        {
          devShells = { };
          formatter = pkgs.nixfmt-rfc-style;
          packages = { };
        };
    };
}
