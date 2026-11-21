# Examples

## `mixamo-root-motion`

These scripts demonstrate adding a skeleton root to a Mixamo FBX animation and extracting root motion along the Z translation axis. They require the skeleton root node to be named `mixamorig:Hips`, create and promote a `Root` parent node, move `mixamorig:Hips` beneath it, and extract its Z-axis translation to that parent. The FBX Rerooter executable must be available on `PATH`, or its path can be supplied through the `FBX_REROOT` environment variable.

### PowerShell

```powershell
mixamo-root-motion.ps1 <InputFile> [OutputFile]
```

### Bash

```bash
mixamo-root-motion.sh <INPUT.fbx> [OUTPUT.fbx]
```

| Parameter                   | Required | Description                                                                                                                           |
| --------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| `InputFile` / `INPUT.fbx`   | Yes      | Path to the source Mixamo FBX file. It must contain a skeleton root named `Hips` (including the common Mixamo `mixamorig:Hips` name). |
| `OutputFile` / `OUTPUT.fbx` | No       | Path for the processed FBX. If omitted, the script writes `<input-basename>_root_motion.fbx` alongside the input file.                |

The scripts use temporary intermediate FBX files and remove them when finished. They overwrite the specified output file without prompting.
