{
  description = "way-displays: Auto Manage Your Wayland Displays";

  # nixpkgs and flake-parts are in the flake registry, so we don't have to specify their input URLs
  # https://github.com/NixOS/flake-registry/blob/master/flake-registry.json
  inputs = {
    pre-commit-hooks-nix = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = inputs@{ nixpkgs, flake-parts, ... }:
    # flake-parts modularizes flake construction
    # https://flake.parts/
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.pre-commit-hooks-nix.flakeModule
      ];

      # It is necessary to explicitly list supported systems. Add more as needed from
      # https://github.com/NixOS/nixpkgs/blob/master/lib/systems/flake-systems.nix
      systems = [ "x86_64-linux" "aarch64-linux" ];
      perSystem = { config, self', inputs', pkgs, system, ... }: {
        # stdenv analyzes the Makefile etc and tries to "do the right thing" without much config
        # https://static.domenkozar.com/nixpkgs-manual-sphinx-exp/stdenv.xml.html
        # This is where the magic happens:
        # https://github.com/NixOS/nixpkgs/blob/master/pkgs/stdenv/generic/setup.sh

        # Use clangStdenv (1.2GB closure) instead of regular gcc stdenv (322MB closure).
        # TODO: Would it be useful to provide both outputs? If not, which should be default?
        # packages.default = pkgs.clangStdenv.mkDerivation {
        packages.default = pkgs.stdenv.mkDerivation {
          name = "way-displays";
          version = "1.7.1+dev";
          src = ./.;
          # native for tools that run on build system (only important for cross compiling)
          nativeBuildInputs = with pkgs; [
            pkg-config
            wayland # need native for protocol code gen
            cmocka
          ];

          # inputs built for target system
          buildInputs = with pkgs; [
            wayland
            yaml-cpp
            libinput
          ];

          makeFlags = [ "CC=clang CXX=clang++ DESTDIR=$(out) PREFIX= PREFIX_ETC= ROOT_ETC=$(out)/etc"];
        };

        # https://ryantm.github.io/nixpkgs/builders/special/mkshell/
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            ccls
            cppcheck
            include-what-you-use
            # config.pre-commit.installationScript
          ];
          inputsFrom = [ self'.packages.default ];
          shellHook = ''
            ${config.pre-commit.installationScript}
            echo 1>&2 "Hello"
          '';
        };

        pre-commit.settings = {
          # https://flake.parts/options/pre-commit-hooks-nix.html#opt-perSystem.pre-commit.settings.hooks
          hooks = {
            editorconfig-checker.enable = true;
          };
          # TODO: cppcheck and iwyu hooks
          # https://github.com/cachix/pre-commit-hooks.nix/issues/275
        };
      };
    };
}
