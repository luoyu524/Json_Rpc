#!/usr/bin/env bash
set -u
cd "$(dirname "$0")"

RES=results
mkdir -p "$RES"
RAW="$RES/raw.csv"
PROBES="$RES/probes.csv"
TOPIC="$RES/topic.csv"
STAB="$RES/stability.csv"

NPROC=$(nproc)
HZ=$(getconf CLK_TCK)
SERVER_CORE=0
if [ "$NPROC" -gt 1 ]; then
    CLIENT_CORES="1-$((NPROC - 1))"
else
    CLIENT_CORES="0"
fi

SPORT=19101
SPORT2=19102
REGPORT=19100
TOPICPORT=19103

echo "tag,threads,shared,method,payload,requests_total,qps,avg_ms,p50_ms,p90_ms,p99_ms,max_ms,errors,server_cpu_pct" > "$RAW"
echo "stage,port,add_count" > "$PROBES"
echo "case,pub_ok,pub_total,pub_qps,sub1_recv,sub2_recv" > "$TOPIC"
echo "case,rss_kb_before,rss_kb_after,fd_before,fd_after,qps" > "$STAB"

PIDS=()
LAST_PID=0

cleanup() {
    if [ "${#PIDS[@]}" -gt 0 ]; then
        for p in "${PIDS[@]}"; do
            kill "$p" 2>/dev/null
        done
        wait 2>/dev/null
    fi
}
trap cleanup EXIT

start_bg() {
    "$@" >/dev/null 2>&1 &
    LAST_PID=$!
    PIDS+=("$LAST_PID")
}

stop_pid() {
    kill "$1" 2>/dev/null
    wait "$1" 2>/dev/null
}

cpu_of() {
    awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null || echo 0
}

rss_of() {
    awk '/VmRSS/{print $2}' "/proc/$1/status" 2>/dev/null || echo 0
}

fd_of() {
    ls "/proc/$1/fd" 2>/dev/null | wc -l
}

probe() {
    local stage="$1"
    local ip="$2"
    local port="$3"
    local out count
    out=$(taskset -c "$CLIENT_CORES" ./bench_client --probe "$ip" "$port" 2>&1 >/dev/null)
    count=$(echo "$out" | awk '{print $3}')
    echo "${stage},${port},${count}" >> "$PROBES"
}

run_case() {
    local tag="$1"
    local threads="$2"
    local shared="$3"
    local method="$4"
    local payload="$5"
    local requests="$6"
    shift 6

    local c0 t0 t1 c1 cpu line
    c0=$(cpu_of "$SPID")
    t0=$(date +%s.%N)
    line=$(timeout 300 taskset -c "$CLIENT_CORES" ./bench_client "$@" \
        --threads "$threads" --shared "$shared" --method "$method" \
        --payload "$payload" --requests "$requests" --tag "$tag" 2>&1 >/dev/null)
    t1=$(date +%s.%N)
    c1=$(cpu_of "$SPID")

    if [ -z "$line" ]; then
        echo "${tag},${threads},${shared},${method},${payload},$((threads * requests)),0,0,0,0,0,0,0,0" >> "$RAW"
        echo "  ${tag}: timeout/failed" >&2
        return
    fi

    cpu=$(awk -v c0="$c0" -v c1="$c1" -v hz="$HZ" -v t0="$t0" -v t1="$t1" \
        'BEGIN{d=(c1-c0)/hz; dt=t1-t0; if(dt<=0)dt=1; printf "%.1f", d/dt*100}')
    echo "${line},${cpu}" >> "$RAW"
    printf '  %s cpu=%s%%\n' "$line" "$cpu"
}

echo "== A. direct RPC =="
start_bg taskset -c "$SERVER_CORE" ./bench_server --port "$SPORT"
SPID=$LAST_PID
sleep 1

for th in 1 4 8 16; do
    run_case "A1_t${th}" "$th" 0 Add 32 20000 --server 127.0.0.1 "$SPORT"
done
run_case "A2_shared" 8 1 Add 32 20000 --server 127.0.0.1 "$SPORT"
run_case "A2_multi" 8 0 Add 32 20000 --server 127.0.0.1 "$SPORT"
run_case "A3_echo1k" 8 0 Echo 1024 10000 --server 127.0.0.1 "$SPORT"
run_case "A3_echo16k" 8 0 Echo 16384 5000 --server 127.0.0.1 "$SPORT"
stop_pid "$SPID"

echo "== B. service discovery =="
start_bg taskset -c "$SERVER_CORE" ./bench_registry --port "$REGPORT"
RPID=$LAST_PID
sleep 1

start_bg taskset -c "$SERVER_CORE" ./bench_server --port "$SPORT" --registry 127.0.0.1 "$REGPORT"
SPID=$LAST_PID
sleep 1

run_case "B1_discovery" 8 0 Add 32 20000 --registry 127.0.0.1 "$REGPORT"
probe "B1_end" 127.0.0.1 "$SPORT"

start_bg taskset -c "$SERVER_CORE" ./bench_server --port "$SPORT2" --registry 127.0.0.1 "$REGPORT"
SPID2=$LAST_PID
sleep 1

run_case "B2_two_providers" 8 0 Add 32 20000 --registry 127.0.0.1 "$REGPORT"
probe "B2_end" 127.0.0.1 "$SPORT"
probe "B2_end" 127.0.0.1 "$SPORT2"

stop_pid "$SPID2"
stop_pid "$SPID"
stop_pid "$RPID"

echo "== C. publish/subscribe =="
start_bg taskset -c "$SERVER_CORE" ./bench_topic_server --port "$TOPICPORT"
TPID=$LAST_PID
sleep 1

taskset -c "$CLIENT_CORES" ./bench_topic_client --role sub --server 127.0.0.1 "$TOPICPORT" \
    --topic bench_topic --duration 25 >/dev/null 2> "$RES/sub1.out" &
SUBPID1=$!
PIDS+=("$SUBPID1")

taskset -c "$CLIENT_CORES" ./bench_topic_client --role sub --server 127.0.0.1 "$TOPICPORT" \
    --topic bench_topic --duration 25 >/dev/null 2> "$RES/sub2.out" &
SUBPID2=$!
PIDS+=("$SUBPID2")

sleep 2
PUBOUT=$(timeout 120 taskset -c "$CLIENT_CORES" ./bench_topic_client --role pub \
    --server 127.0.0.1 "$TOPICPORT" --topic bench_topic --count 50000 --payload 64 2>&1 >/dev/null)
wait "$SUBPID1"
wait "$SUBPID2"

SUB1=$(awk '/SUB_RECEIVED/{print $2}' "$RES/sub1.out")
SUB2=$(awk '/SUB_RECEIVED/{print $2}' "$RES/sub2.out")
PUB_OK=$(echo "$PUBOUT" | awk '{print $2}')
PUB_TOTAL=$(echo "$PUBOUT" | awk '{print $4}')
PUB_QPS=$(echo "$PUBOUT" | awk '{print $8}')
echo "C1_topic_2subs,${PUB_OK},${PUB_TOTAL},${PUB_QPS},${SUB1},${SUB2}" >> "$TOPIC"
echo "  $PUBOUT sub1=$SUB1 sub2=$SUB2"
stop_pid "$TPID"

echo "== D. short stability =="
start_bg taskset -c "$SERVER_CORE" ./bench_server --port "$SPORT"
SPID=$LAST_PID
sleep 1

RSS0=$(rss_of "$SPID")
FD0=$(fd_of "$SPID")
run_case "D1_long" 8 0 Add 32 50000 --server 127.0.0.1 "$SPORT"
RSS1=$(rss_of "$SPID")
FD1=$(fd_of "$SPID")
QPS=$(tail -n 1 "$RAW" | awk -F, '{print $7}')
echo "D1_long,${RSS0},${RSS1},${FD0},${FD1},${QPS}" >> "$STAB"
stop_pid "$SPID"

echo "done. results in $RES/"
