param([int]$Seconds = 3)

$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
Write-Output "PowerShell demo started."
for ($Index = 1; $Index -le $Seconds; $Index++) {
    Write-Output "PowerShell progress: $Index/$Seconds"
    Start-Sleep -Seconds 1
}
Write-Output "PowerShell demo completed."
