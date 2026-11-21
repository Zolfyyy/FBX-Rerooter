#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 INPUT.fbx [OUTPUT.fbx]" >&2
    exit 2
}

[[ $# -ge 1 && $# -le 2 ]] || usage
input=$1
output=${2:-"${input%.*}_root_motion.fbx"}

if [[ ! -f "$input" ]]; then
    echo "Error: input FBX '$input' does not exist." >&2
    exit 1
fi

fbx_reroot=${FBX_REROOT:-fbx-reroot}
command -v "$fbx_reroot" >/dev/null 2>&1 || {
    echo "Error: fbx-reroot executable not found; set FBX_REROOT or add it to PATH." >&2
    exit 1
}

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
created="$work_dir/created.fbx"
rooted="$work_dir/rooted.fbx"
moved="$work_dir/moved.fbx"
normalized="$work_dir/normalized.fbx"

hips_path=
while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    path=${path%$'\r'}
    leaf=${path##*,}
    if [[ "$leaf" == mixamorig:Hips ]]; then
        hips_path=$path
        break
    fi
done < <("$fbx_reroot" find -t "$input")

if [[ -z "$hips_path" ]]; then
    echo "Error: no skeleton root named 'mixamorig:Hips' found in '$input'." >&2
    exit 1
fi

root_name=Root

parent_path=
if [[ "$hips_path" == *,* ]]; then
    parent_path=${hips_path%,*}
fi
if [[ -n "$parent_path" ]]; then
    root_path="$parent_path,$root_name"
else
    root_path=$root_name
fi
new_hips_path="$root_path,mixamorig:Hips"

"$fbx_reroot" create "$input" "$root_path" -o "$created" -y
"$fbx_reroot" make-root "$created" --to-node "$root_name" -o "$rooted" -y
"$fbx_reroot" move "$rooted" "$hips_path" "$new_hips_path" -o "$moved" -y
# Re-run make-root after the move so the former Hips root becomes a limb under the new root.
"$fbx_reroot" make-root "$moved" --to-node "$root_name" -o "$normalized" -y
"$fbx_reroot" extract-motion "$normalized" "$new_hips_path" --translation z -o "$output" -y
