{
  # package to override
  yara
, # deps
  cmake
, pkg-config
}:
yara.overrideAttrs (old: {
  dontUseCmakeConfigure = true;
  nativeBuildInputs = old.nativeBuildInputs ++ [ cmake pkg-config ];
  postInstall = old.postInstall or "" + ''
    mkdir -p $out/lib/cmake/unofficial-libyara
    cp ${./yara-config.cmake} $out/lib/cmake/unofficial-libyara/unofficial-libyaraConfig.cmake
  '';
})
