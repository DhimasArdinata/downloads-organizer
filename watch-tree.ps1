# --- CONFIGURATION ---
$folderToWatch = $PSScriptRoot
$outputFile = Join-Path $folderToWatch "tree-view.txt"
$outputFileName = Split-Path $outputFile -Leaf

# --- NEW: List of top-level folders to show but not expand ---
$closedFolders = @(".git", ".vscode", "build")

$fileComments = @{
    "main.cpp"      = "# The main entry point (now very small)"
    "types.hpp"     = "# All our custom data structures"
    "IOManager.hpp" = "# Handles loading config, finding folders, undo"
    "IOManager.cpp" = "# Implementation for File I/O"
    "RuleEngine.hpp"= "# The core analysis and planning logic"
    "RuleEngine.cpp"= "# Implementation of the analysis engine"
    "UI.hpp"        = "# The entire interactive TUI"
    "UI.cpp"        = "# Implementation of the TUI"
}

# --- CORE LOGIC ---
function Generate-Tree {
    Write-Host "Change detected. Regenerating tree view..."
    $treeOutput = @()
    $treeOutput += "folder-organizer/"

    # Get all items, excluding only the output file itself for now.
    $items = Get-ChildItem -Path $folderToWatch -Recurse -Exclude $outputFileName

    foreach ($item in $items) {
        # --- NEW LOGIC: Check if the item is inside a "closed" folder ---
        $isInsideClosedFolder = $false
        $relativePath = $item.FullName.Substring($folderToWatch.Length + 1)
        foreach ($closedFolder in $closedFolders) {
            # If the path starts with a closed folder name followed by a slash, it's a child item.
            if ($relativePath.StartsWith("$closedFolder\") -or $relativePath.StartsWith("$closedFolder/")) {
                $isInsideClosedFolder = $true
                break
            }
        }
        # If it's inside, skip to the next item in the loop.
        if ($isInsideClosedFolder) {
            continue
        }

        # --- Original logic continues for all other items ---
        $depth = ($relativePath -split '[\\/]').Count - 1
        $indent = "    " * $depth
        $prefix = "|-- " 
        
        $line = "$indent$prefix$($item.Name)"

        if ($fileComments.ContainsKey($item.Name)) {
            $line = $line.PadRight(40) + $fileComments[$item.Name]
        }
        
        $treeOutput += $line
    }

    Set-Content -Path $outputFile -Value ($treeOutput | Out-String)
    Write-Host "Tree view updated in '$outputFile'."
}

# --- FILE SYSTEM WATCHER SETUP (Unchanged) ---
$watcher = New-Object System.IO.FileSystemWatcher
$watcher.Path = $folderToWatch
$watcher.IncludeSubdirectories = $true
$watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName, DirectoryName, LastWrite'

$action = {
    Start-Sleep -Seconds 1
    Generate-Tree
}

Register-ObjectEvent $watcher "Created" -Action $action | Out-Null
Register-ObjectEvent $watcher "Deleted" -Action $action | Out-Null
Register-ObjectEvent $watcher "Changed" -Action $action | Out-Null
Register-ObjectEvent $watcher "Renamed" -Action $action | Out-Null

# --- INITIAL RUN & INFINITE LOOP (Unchanged) ---
Generate-Tree

Write-Host "Watcher started. This script will now run silently in the background."
while ($true) {
    Start-Sleep -Seconds 60
}