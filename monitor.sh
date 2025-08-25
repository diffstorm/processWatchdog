#!/bin/bash
# monitor.sh <cpu_max> <cpu_avg> <mem_max_MB> <mem_avg_MB> <duration_sec> <cmd>

cpu_max=$1
cpu_avg=$2
mem_max=$3
mem_avg=$4
duration=$5
shift 5
cmd="$@"

start_time=$(date +%s)
end_time=$((start_time + duration))

# Start the command
$cmd &
pid=$!
echo "Monitoring PID $pid for $duration seconds..."
sleep 5

# Cleanup handler
cleanup() {
    echo "Cleaning up... killing $pid"
    kill -SIGUSR1 $pid 2>/dev/null
}
trap cleanup EXIT

# Stats accumulators
cpu_sum=0
cpu_count=0
cpu_peak=0
mem_sum=0
mem_peak=0

last_minute=$start_time

while kill -0 $pid 2>/dev/null && [ $(date +%s) -lt $end_time ]; do
    # Get stats
    stats=$(ps -p $pid -o %cpu= -o rss= -o nlwp= 2>/dev/null)
    cpu=$(echo $stats | awk '{print $1}')
    mem_kb=$(echo $stats | awk '{print $2}')
    threads=$(echo $stats | awk '{print $3}')
    mem_mb=$(echo "scale=2; $mem_kb / 1024" | bc)

    # Update accumulators
    cpu_sum=$(echo "$cpu_sum + $cpu" | bc)
    cpu_count=$((cpu_count + 1))
    (( $(echo "$cpu > $cpu_peak" | bc -l) )) && cpu_peak=$cpu
    (( $(echo "$mem_mb > $mem_peak" | bc -l) )) && mem_peak=$mem_mb
    mem_sum=$(echo "$mem_sum + $mem_mb" | bc)

    # Per-second live print
    # printf "[LIVE] CPU=%.2f%% MEM=%.2fMB Threads=%s\n" "$cpu" "$mem_mb" "$threads"

    now=$(date +%s)
    # Every 60s print summary
    if (( now - last_minute >= 60 )); then
        avg_cpu=$(echo "scale=2; $cpu_sum / $cpu_count" | bc)
        avg_mem=$(echo "scale=2; $mem_sum / $cpu_count" | bc)
        echo "=== Minute Summary ($(date '+%Y-%m-%d %H:%M:%S')) ==="
        printf "Avg CPU: %.2f%%   Max CPU: %.2f%%\n" "$avg_cpu" "$cpu_peak"
        printf "Avg MEM: %.2fMB   Max MEM: %.2fMB\n" "$avg_mem" "$mem_peak"
        echo "Threads: $threads"
        last_minute=$now
    fi

    sleep 5
done

# Final summary
avg_cpu=$(echo "scale=2; $cpu_sum / $cpu_count" | bc)
avg_mem=$(echo "scale=2; $mem_sum / $cpu_count" | bc)
echo "=== Final Results ==="
printf "Avg CPU: %.2f%%   Max CPU: %.2f%% (limit avg=%.2f, max=%.2f)\n" "$avg_cpu" "$cpu_peak" "$cpu_avg" "$cpu_max"
printf "Avg MEM: %.2fMB   Max MEM: %.2fMB (limit avg=%.2f, max=%.2f)\n" "$avg_mem" "$mem_peak" "$mem_avg" "$mem_max"

# Check limits
fail=0
if (( $(echo "$avg_cpu >= $cpu_avg" | bc -l) )); then
    echo "FAIL: Average CPU ${avg_cpu}% exceeds limit ${cpu_avg}%"
    fail=1
fi
if (( $(echo "$cpu_peak >= $cpu_max" | bc -l) )); then
    echo "FAIL: Max CPU ${cpu_peak}% exceeds limit ${cpu_max}%"
    fail=1
fi
if (( $(echo "$avg_mem >= $mem_avg" | bc -l) )); then
    echo "FAIL: Average MEM ${avg_mem}MB exceeds limit ${mem_avg}MB"
    fail=1
fi
if (( $(echo "$mem_peak >= $mem_max" | bc -l) )); then
    echo "FAIL: Max MEM ${mem_peak}MB exceeds limit ${mem_max}MB"
    fail=1
fi

if [ $fail -eq 0 ]; then
    echo "PASS: Resource usage within limits"
fi

exit $fail
