$outputFile = "ipaddress.txt"
Set-Content -Path $outputFile -Value $null

$subnets = Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notlike "169.*" -and $_.IPAddress -ne "127.0.0.1" -and $_.PrefixLength -eq 24 } |
    Select-Object -ExpandProperty IPAddress

foreach ($ip in $subnets) {
    $baseIP = ($ip -split '\.')[0..2] -join '.'
    Write-Host "Scanning subnet: $baseIP.0/24"

    1..254 | ForEach-Object -ThrottleLimit 50 -Parallel {
        $addr = "$using:baseIP.$_"
        if (ping.exe -n 1 -w 200 $addr | Select-String "TTL=") {
            $addr
        }
    } | ForEach-Object {
        Write-Host "Alive: $_"
        Add-Content -Path $outputFile -Value $_
    }
}

