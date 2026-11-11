{
  description = "nexus - a small, fast spinning 3D terminal fetch tool";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = nixpkgs.legacyPackages.${system};
      });
    in
    {
      packages = forAllSystems ({ pkgs, ... }: rec {
        nexus = pkgs.stdenv.mkDerivation {
          pname = "nexus";
          version = "1.0.0";
          src = self;
          makeFlags = [ "PREFIX=${placeholder "out"}" ];
          meta = with pkgs.lib; {
            description = "A small, fast spinning 3D terminal fetch tool";
            homepage = "https://github.com/campbell-nick/nexus";
            license = licenses.mit;
            platforms = platforms.unix;
            mainProgram = "nexus";
          };
        };
        default = nexus;
      });

      devShells = forAllSystems ({ pkgs, ... }: {
        default = pkgs.mkShell {
          packages = [ pkgs.gcc pkgs.gnumake ];
        };
      });
    };
}
