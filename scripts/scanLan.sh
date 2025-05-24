#!/bin/bash

outputFile="ipAddress.txt"
> "$outputFile"


# getting address of subnet /24 like i do for ps1 script, because thats what my router shows everyone under
ip_info=$(ip -o -f inet addr show | awk '$4 ~ /\/24/ && $2 != "lo" && $4 !~ /^169/ {print $4}' | head -n 1)
if [[ -z "$ip_info" ]]; then
    echo "No /24 IPv4 address found"
    exit 1
fi

baseIP=$(echo "$ip_info" | cut -d/ -f1 | awk -F. '{print $1"."$2"."$3}')
echo "Scanning subnet: $baseIP.0/24"

ping_ip() {
    local ip=$1
    if timeout 1 ping -c 1 "$ip" > /dev/null 2>&1; then
        echo "$ip" >> "$outputFile"
        echo "Alive: $ip"
    fi
}

# Limit parallel jobs
limit_jobs() {
    while [[ $(jobs -r -p | wc -l) -ge 40 ]]; do
        sleep 0.1
    done
}

# Ping all IPs in the /24 subnet
for i in $(seq 1 254); do
    ip="$baseIP.$i"
    ping_ip "$ip" &
    limit_jobs
done

wait

echo "Scan complete. Results saved to $outputFile"
