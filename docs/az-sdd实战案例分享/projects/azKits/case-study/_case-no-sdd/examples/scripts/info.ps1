[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$os = Get-CimInstance Win32_OperatingSystem
Write-Output "计算机名: $env:COMPUTERNAME"
Write-Output "系统版本: $($os.Caption) $($os.Version)"
Write-Output "[OK] PowerShell 脚本运行完成"