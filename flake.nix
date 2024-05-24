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
          pkg-config
          cmake
          ninja
        ];
        buildInputs = with pkgs; [
          sqlite-interactive.dev
          curl.dev
          json_c.dev
        ];
      };

      packages.default = pkgs.stdenv.mkDerivation {
        pname = "sqlite-mc-uuid";
        version = "1.0.0";

        src = ./.;

        nativeBuildInputs = with pkgs; [
          pkg-config
          cmake
          ninja
        ];

        buildInputs = with pkgs; [
          sqlite
          curl
          json_c
        ];

        installPhase = ''
          install -D ./libsqlite_mc_uuid.so $out/lib/libsqlite_mc_uuid.so
        '';
      };
    });
}
