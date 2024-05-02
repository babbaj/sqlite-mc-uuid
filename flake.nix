{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem(system:
    let
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          clang_16
          pkg-config
          cmake
          ninja
        ];
        buildInputs = with pkgs; [
          sqlite-interactive.dev
          curl.dev
          json_c.dev
        ];

        shellHook = ''
          export CC=${pkgs.clang_16}/bin/clang
          export CXX=${pkgs.clang_16}/bin/clang++
        '';
      };
    });
}
