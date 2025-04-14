{
  # deps
  cmake
, ninja
, # helpers
  lib
, stdenv
, # source files
  source
}:
stdenv.mkDerivation {
  pname = "bext-di";
  version = "v1.3.2";

  src = source;

  nativeBuildInputs = [ cmake ninja ];

  cmakeFlags = [
    "-DBOOST_DI_OPT_BUILD_TESTS=OFF"
    "-DBOOST_DI_OPT_BUILD_EXAMPLES=OFF"
    "-DBOOST_DI_OPT_INSTALL=ON"
  ];

  meta = with lib; {
    description = "A header-only dependency injection library for C++";
    license = licenses.mit;
    platforms = platforms.all;
  };
}
