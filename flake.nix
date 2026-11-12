{
  description = "nexus - a spinning 3D terminal fetch tool";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "armv7l-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # Read NEXUS_VERSION out of src/nexus.h so the package version never drifts.
      version =
        let
          lines = nixpkgs.lib.splitString "\n" (builtins.readFile ./src/nexus.h);
          line = nixpkgs.lib.findFirst
            (l: nixpkgs.lib.hasPrefix "#define NEXUS_VERSION" l)
            "" lines;
          m = builtins.match ''#define NEXUS_VERSION "([^"]+)"'' line;
        in
        if m == null then "0.0.0" else builtins.head m;

      mkNexus = pkgs: extraMakeFlags: pkgs.stdenv.mkDerivation {
        pname = "nexus";
        inherit version;
        src = ./.;

        # Plain Makefile project: no configure step.
        dontConfigure = true;
        makeFlags = extraMakeFlags;
        installFlags = [ "PREFIX=${placeholder "out"}" ];

        meta = with nixpkgs.lib; {
          description = "A spinning 3D terminal fetch tool with system info";
          homepage = "https://github.com/campbell-nick/nexus";
          license = licenses.mit;
          platforms = platforms.unix;
          mainProgram = "nexus";
        };
      };
    in
    {
      packages = forAllSystems (pkgs:
        {
          default = mkNexus pkgs [ ];
        }
        // nixpkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
          # Same source linked against musl, statically.
          musl = mkNexus pkgs.pkgsMusl [ "STATIC=1" ];
        });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            gcc
            clang
            gnumake
          ];
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixpkgs-fmt);
    };
}
