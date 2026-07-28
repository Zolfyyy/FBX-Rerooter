<img src="./banner.png" align="center"/>
<h1 align="center">FBX Rerooter</h1>
<p align="center">A command-line utility for re-rooting skeletons in FBX files using the Autodesk FBX SDK.</p>

## Why It Exists

When exporting models or animations from Unity Editor, the FBX exporter often excludes root motion bones of a skeleton from the bone hierarchy itself since the root motion bone _usually_ doesn't carry any weights to any skinned mesh renderers. This makes it problematic when exporting skeletons or animations with root motion, often having to reconstruct the root motion bone in an external program like Blender and transferring the root motion into it.

To solve this, this tool inspects the FBX skeleton hierarchy and can promote any parent node to a Skeleton root node.

## Usage

```
fbx-reroot [OPTIONS] SUBCOMMAND
```

All examples below use the test file `test/Skeleton.fbx` which ships with the repository.

---

### `find`

Lists all nodes in the FBX file that carry a `Skeleton` attribute of type `Root`.

```bash
fbx-reroot find test/Skeleton.fbx
```

**Output:**
```
Hips
```

Use `--tree` to show the full ancestor path from the scene root to each skeleton root node (comma-separated):

```bash
fbx-reroot find -t test/Skeleton.fbx
```

**Output:**
```
Skeleton,Root,Hips
```

This tells us `Hips` is the current skeleton root, and its full hierarchy is `Skeleton → Root → Hips`.

---

### `make-root`

Promotes a node to a `Skeleton` root. Two modes are available.

#### Mode A: Promote a specific node (`--to-node`)

Promote a named node to skeleton root, optionally demoting an existing child skeleton root to a limb:

```bash
fbx-reroot make-root test/Skeleton.fbx --to-node Root --from-node Hips -o test/Skeleton_rerooted.fbx -y
```

**Output:**
```
Converted 'Root' to a Skeleton root node.
Wrote 'test\Skeleton_rerooted.fbx'.
```

Verify the result:

```bash
fbx-reroot find -t test/Skeleton_rerooted.fbx
```

**Output:**
```
Skeleton,Root
```

`Root` is now the skeleton root, and `Hips` was demoted to a limb node. (The `-y` flag skips the overwrite prompt.)

#### Mode B: Walk up from an existing root (`--from-node` + `--to-parent`)

Walk up `N` levels from an existing skeleton root and make that ancestor the new root:

```bash
fbx-reroot make-root test/Skeleton.fbx --from-node Hips --to-parent 1 -o test/Skeleton_rerooted.fbx -y
```

Given the hierarchy `Skeleton,Root,Hips`, walking up 1 level from `Hips` reaches `Root`, which becomes the new skeleton root.

#### Common options

| Option                | Description                                                   |
| --------------------- | ------------------------------------------------------------- |
| `-o, --output <path>` | Output FBX file path (default: `<input>` in CWD)              |
| `-f, --format <fmt>`  | Output format: `auto`, `ascii`, or `binary` (default: `auto`) |
| `-y, --yes`           | Skip the overwrite confirmation prompt                        |

**Output format notes:**
- `auto` - matches the input file's format (binary or ASCII).
- `ascii` - forces ASCII FBX (human-readable, larger files).
- `binary` - forces binary FBX (smaller, faster to load).

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler (MSVC, Clang, GCC)
- [Autodesk FBX SDK](https://aps.autodesk.com/developer/overview/fbx-sdk) (2020.3.x or later)

### Configure & build

```bash
cmake -B build -DFBX_SDK_DIR="C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.9"
cmake --build build
```

## License

MIT