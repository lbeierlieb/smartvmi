{ pkgs, yara-cmake }:
{
  apitracing = pkgs.callPackage ./build_plugin.nix {
    name = "apitracing";
  };
  inmemoryscanner = pkgs.callPackage ./build_plugin.nix {
    name = "inmemoryscanner";
    additionalDeps = [ yara-cmake ];
  };
}
