# tcHarmonicDeformer Test Setup (Maya 2026)

This document provides a quick test setup for `tcHarmonicDeformer` after building:

- Plugin binary:
  `E:\dev\MAYA\API\C++\Autodesk_Maya_2026_3_Update_DEVKIT_Windows\devkitBase\build\harmonicDef\Release\tcHarmonicDeformer.mll`

## 1) Build (PowerShell)

```powershell
$env:DEVKIT_LOCATION='E:\dev\MAYA\API\C++\Autodesk_Maya_2026_3_Update_DEVKIT_Windows\devkitBase'
& 'C:\Program Files\CMake\bin\cmake.exe' -S "$env:DEVKIT_LOCATION\devkit\plug-ins\MatchMesh\harmonicDef" -B "$env:DEVKIT_LOCATION\build\harmonicDef" -G "Visual Studio 17 2022" -A x64
& 'C:\Program Files\CMake\bin\cmake.exe' --build "$env:DEVKIT_LOCATION\build\harmonicDef" --config Release
```

## 2) Maya Python Test (Script Editor)

Paste in Maya Script Editor (Python tab):

```python
import maya.cmds as cmds

plugin_path = r"E:\dev\MAYA\API\C++\Autodesk_Maya_2026_3_Update_DEVKIT_Windows\devkitBase\build\harmonicDef\Release\tcHarmonicDeformer.mll"
plugin_name = "tcHarmonicDeformer"

cmds.file(new=True, force=True)

try:
    is_loaded = cmds.pluginInfo(plugin_name, q=True, loaded=True)
except RuntimeError:
    is_loaded = False

if not is_loaded:
    cmds.loadPlugin(plugin_path, quiet=True)

# Create test geometry: model first, cage second
model = cmds.polySphere(r=1.0, sx=24, sy=24, name="harmonicModel")[0]
cage = cmds.polyCube(w=3.0, h=3.0, d=3.0, sx=1, sy=1, sz=1, name="harmonicCage")[0]

cmds.select(clear=True)
cmds.select(model, cage, r=True)
deformer = cmds.tcCreateHarmonicDeformer()

# Optional: remove the auto parentConstraint created by tcCreateHarmonicDeformer
# so you can see pure cage-shape deformation only.
constraints = cmds.listConnections(model, type="parentConstraint") or []
if constraints:
    cmds.delete(constraints)

# Optional tuning
cmds.setAttr(f"{deformer}.cellSize", 0.5)
cmds.setAttr(f"{deformer}.maxIterations", 15)
cmds.setAttr(f"{deformer}.threshold", 0.00001)
cmds.setAttr(f"{deformer}.dynamicBinding", 0)

# Compute harmonic weights
cmds.tcComputeHarmonicWeights(d=deformer, mi=15, cs=0.5, ts=0.00001, sg=False)

# Move cage to validate deformation
cmds.move(0.3, 0.0, 0.0, f"{cage}.vtx[*]", r=True, ws=True)

print("Harmonic deformer test setup complete:", deformer)
```

## 3) Maya MEL Test (Script Editor)

Paste in Maya Script Editor (MEL tab):

```mel
file -f -new;

loadPlugin "E:/dev/MAYA/API/C++/Autodesk_Maya_2026_3_Update_DEVKIT_Windows/devkitBase/build/harmonicDef/Release/tcHarmonicDeformer.mll";

polySphere -r 1.0 -sx 24 -sy 24 -n "harmonicModel";
polyCube -w 3.0 -h 3.0 -d 3.0 -sx 1 -sy 1 -sz 1 -n "harmonicCage";

select -r harmonicModel harmonicCage;
string $def = `tcCreateHarmonicDeformer`;

setAttr ($def + ".cellSize") 0.5;
setAttr ($def + ".maxIterations") 15;
setAttr ($def + ".threshold") 0.00001;
setAttr ($def + ".dynamicBinding") 0;

tcComputeHarmonicWeights -d $def -mi 15 -cs 0.5 -ts 0.00001 -sg 0;

move -r -ws 0.3 0.0 0.0 "harmonicCage.vtx[*]";

print ("Harmonic deformer test setup complete: " + $def + "\n");
```

## Notes

- Selection order matters for `tcCreateHarmonicDeformer`: select **model first**, then **cage**.
- Cage mesh must be closed and use triangle/quad faces.
- If you change topology on either mesh, recompute weights.
- In Python tab, use `cmds.select(clear=True)`. Do not use MEL syntax like `select -cl;` in Python code.
- `tcCreateHarmonicDeformer` also creates a `parentConstraint` from cage transform to model transform. If you move cage in object mode, the model may follow because of this constraint (not because of harmonic weights).
- To test harmonic deformation itself, move cage **components** (for example `cage.vtx[*]`) or delete the auto `parentConstraint`.
