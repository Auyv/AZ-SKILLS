param([int]$Count = 3)
Write-Output "PowerShell 进度演示, Count=$Count"
for ($i = 1; $i -le $Count; $i++) {
    Write-Output "进度 $i/$Count"
}
Write-Output "PowerShell 完成"
