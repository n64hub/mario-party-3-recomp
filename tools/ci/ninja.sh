#!/usr/bin/env bash
# ninja with its arguments, run again when a compile failed only because sccache's client could not connect to its
# server. On Windows a connect to 127.0.0.1 can fail with "Only one usage of each socket address ... (os error 10048)"
# when the local port it was handed is still in use; sccache (0.20) retries a refused or timed-out connect, and starts
# a server for one, but hands this error back as the compile's own. The server is fine, so another go of ninja, which
# reruns only what failed, gets past it. Any other failure exits at once with ninja's code.
log=$(mktemp)
trap 'rm -f "$log"' EXIT
for i in 1 2 3; do
  ninja "$@" 2>&1 | tee "$log"
  rc=${PIPESTATUS[0]}
  [ "$rc" = 0 ] && exit 0
  grep -q 'os error 10048' "$log" || exit "$rc"
  echo "::warning::sccache could not connect to its server (os error 10048), ninja attempt $i of 3"
done
exit "$rc"
