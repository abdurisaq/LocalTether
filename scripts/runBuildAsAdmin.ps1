# run_build_as_admin.ps1

# Check if running as admin
function Is-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Is-Admin)) {
    Write-Host "Not running as admin. Elevating..."
    Start-Process powershell.exe "-NoExit -ExecutionPolicy Bypass -File `"$PSScriptRoot\build.ps1`"" -Verb RunAs
    exit
}

# If already elevated, just run the script
& "$PSScriptRoot\build.ps1"

