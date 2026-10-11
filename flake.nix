{
  description = "WICKLIGHT - native Linux 3D pixel action-adventure";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      runtimeLibs = with pkgs; [ SDL2 libGL ];
    in {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "wicklight";
        version = "1.0.0";
        src = ./.;
        nativeBuildInputs = [ pkgs.cmake pkgs.makeWrapper ];
        # SDL2 and OpenGL are loaded at runtime with dlopen, so point the loader at them.
        postInstall = ''
          wrapProgram $out/bin/wicklight --prefix LD_LIBRARY_PATH : ${pkgs.lib.makeLibraryPath runtimeLibs}:/run/opengl-driver/lib
        '';
      };
      apps.${system}.default = { type = "app"; program = "${self.packages.${system}.default}/bin/wicklight"; };
      devShells.${system}.default = pkgs.mkShell {
        packages = [ pkgs.gcc pkgs.cmake ];
        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath runtimeLibs + ":/run/opengl-driver/lib";
      };
    };
}
