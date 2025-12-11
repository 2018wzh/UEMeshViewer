# UEMeshViewer

OBJ spectator viewer (UE5 + vcpkg ready).

## Features
- Reads the command-line arg `OBJ=<absolute or relative path>` at startup and loads that OBJ.
- Builds geometry with `ProceduralMeshComponent`; auto-computes tangents if normals are missing.
- Uses spectator mode (`SpectatorPawn`) and positions the view above/aside the mesh center.

## Run
1) Open `UEMeshViewer.uproject` (or regenerate VS project files).
2) Launch with an OBJ path:
	 ```pwsh
	 # Editor in game/spectator mode
	 & "${env:ProgramFiles}\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe" `
		 "${PWD}\\UEMeshViewer.uproject" `
		 -game OBJ="D:/models/teapot.obj"
	 ```
3) To avoid passing the flag, set `DefaultObjPath` in `Config/DefaultGame.ini`.

## vcpkg notes
- If you want third-party mesh tooling (e.g., `assimp`, `meshoptimizer`):
	```pwsh
	vcpkg install assimp:x64-windows meshoptimizer:x64-windows
	```
- Use `vcpkg integrate project` so generated VS projects inherit include/lib paths, or wire them in `.Build.cs` via `PublicSystemIncludePaths` and `PublicAdditionalLibraries` pointing at `<vcpkg_root>/installed/x64-windows`. The current sample runs without extra packages.