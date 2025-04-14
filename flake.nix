{
  description = "smartvmi";

  inputs = {
    crane.url = "github:ipetkov/crane/master";
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    nixpkgs-24-05.url = "github:NixOS/nixpkgs/nixos-24.05";
    # source for boost-ext DI. When updating URL to a newer version here,
    # remember to also update the version attribute in the bext-di derivation.
    bext-di = {
      url = "github:boost-ext/di?ref=v1.3.2";
      flake = false;
    };
    libvmi-gdata = {
      url = "github:GDATASoftwareAG/libvmi?rev=224b204db82f8648bdd475f2b8a48aa4de143c97";
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
          old-pkgs = import inputs.nixpkgs-24-05 { inherit system; };
          old-libvirt = old-pkgs.libvirt;
          bext-di = pkgs.callPackage ./nix/build_bext-di.nix { source = inputs.bext-di; };
          yara-cmake = pkgs.callPackage ./nix/build_yara-cmake.nix { };
          libvmi-gdata = pkgs.callPackage ./nix/build_libvmi-gdata.nix {
            source = inputs.libvmi-gdata;
            # libvirt version has to match the version of the libvirt running the VM
            libvirt = old-libvirt;
          };
        in
        {
          devShells = { };
          formatter = pkgs.nixfmt-rfc-style;
          packages =
            let
              vmicore = pkgs.callPackage ./nix/build_vmicore.nix {
                inherit bext-di;
                craneLib = inputs.crane.mkLib pkgs;
                libvmi = libvmi-gdata;
              };
            in
            {
              default = pkgs.symlinkJoin {
                name = "smartvmi";
                paths = [
                  vmicore
                ];
              };
              inherit vmicore;
            };
        };
    };
}
