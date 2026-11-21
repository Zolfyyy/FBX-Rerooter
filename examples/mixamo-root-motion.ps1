[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$InputFile,

    [Parameter(Position = 1)]
    [string]$OutputFile
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputFile -PathType Leaf)) {
    throw "Input FBX '$InputFile' does not exist."
}
if (-not $OutputFile) {
    $inputItem = Get-Item -LiteralPath $InputFile
    $OutputFile = Join-Path $inputItem.DirectoryName ($inputItem.BaseName + '_root_motion.fbx')
}

$fbxReroot = if ($env:FBX_REROOT) { $env:FBX_REROOT } else { 'fbx-reroot' }
if (-not (Get-Command $fbxReroot -ErrorAction SilentlyContinue)) {
    throw "fbx-reroot executable not found; set FBX_REROOT or add it to PATH."
}

$tempDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $tempDirectory | Out-Null
$created = Join-Path $tempDirectory 'created.fbx'
$rooted = Join-Path $tempDirectory 'rooted.fbx'
$moved = Join-Path $tempDirectory 'moved.fbx'
$normalized = Join-Path $tempDirectory 'normalized.fbx'

try {
    $rootPaths = & $fbxReroot find -t $InputFile
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect skeleton roots in '$InputFile'."
    }

    $hipsPath = $null
    foreach ($path in $rootPaths) {
        $path = [string]$path
        $leaf = ($path -split ',')[-1]
        if ($path -and $leaf -ceq 'mixamorig:Hips') {
            $hipsPath = $path
            break
        }
    }
    if (-not $hipsPath) {
        throw "No skeleton root named 'mixamorig:Hips' found in '$InputFile'."
    }

    $rootName = 'Root'
    $lastSeparator = $hipsPath.LastIndexOf(',')
    $parentPath = if ($lastSeparator -ge 0) { $hipsPath.Substring(0, $lastSeparator) } else { '' }
    $rootPath = if ($parentPath) { "$parentPath,$rootName" } else { $rootName }
    $newHipsPath = "$rootPath,mixamorig:Hips"

    & $fbxReroot create $InputFile $rootPath -o $created -y
    if ($LASTEXITCODE -ne 0) { throw 'fbx-reroot create failed.' }

    & $fbxReroot make-root $created --to-node $rootName -o $rooted -y
    if ($LASTEXITCODE -ne 0) { throw 'fbx-reroot make-root failed.' }

    & $fbxReroot move $rooted $hipsPath $newHipsPath -o $moved -y
    if ($LASTEXITCODE -ne 0) { throw 'fbx-reroot move failed.' }

    # Re-run make-root after the move so the former Hips root becomes a limb under the new root.
    & $fbxReroot make-root $moved --to-node $rootName -o $normalized -y
    if ($LASTEXITCODE -ne 0) { throw 'fbx-reroot make-root failed after moving Hips.' }

    & $fbxReroot extract-motion $normalized $newHipsPath --translation z -o $OutputFile -y
    if ($LASTEXITCODE -ne 0) { throw 'fbx-reroot extract-motion failed.' }
}
finally {
    Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
