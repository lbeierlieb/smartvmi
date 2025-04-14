{
  description = "smartvmi";

  inputs = {
    crane.url = "github:ipetkov/crane/master";
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
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
        {
          devShells = { };
          formatter = pkgs.nixfmt-rfc-style;
          packages = { };
        };
    };
}
