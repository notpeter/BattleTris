#!/bin/sh
# A fixed Xvfb layout makes these coordinates reproducible. This is a native
# startup/play/pause smoke check, not a deterministic gameplay parity test.
set -eu
export DISPLAY=:99
mkdir -p /tmp/native-results
Xvfb "$DISPLAY" -screen 0 1280x1024x24 > /tmp/native-results/xvfb.log 2>&1 &
xpid=$!
gamepid=
cleanup() {
    if test -n "$gamepid"; then
        kill "$gamepid" 2>/dev/null || :
        wait "$gamepid" 2>/dev/null || :
    fi
    kill "$xpid" 2>/dev/null || :
}
trap cleanup EXIT
ready=0
for _attempt in 1 2 3 4 5; do
    if xdpyinfo >/dev/null 2>&1; then ready=1; break; fi
    sleep 1
done
test "$ready" = 1
cd /work/src/daemons
./btserverd -f ./btserver.cf -n 1
cd /work/src/game
sleep 1
./BattleTris -m -S localhost > /tmp/native-results/game.log 2>&1 &
gamepid=$!
xdotool search --sync --onlyvisible --name BattleTris >/dev/null
sleep 1
kill -0 "$gamepid"
import -window root /tmp/native-results/start.png
xdotool mousemove 225 500 click 1
sleep 1
import -window root /tmp/native-results/challenge.png
xdotool mousemove 165 486 click 1
# Allow two placements by Comatose Ernie (four seconds each), including
# activation of the initial queued Condor request and a later report.
sleep 9
xdotool mousemove 150 300 key p
sleep 1
import -window root /tmp/native-results/paused.png
sleep 1
import -window root /tmp/native-results/still-paused.png
compare -metric AE /tmp/native-results/paused.png /tmp/native-results/still-paused.png null: 2>/tmp/native-results/pause-difference.txt
xdotool key p
sleep 2
import -window root /tmp/native-results/resumed.png
# A frozen or missing game must not pass just because two screenshots match.
if compare -metric AE /tmp/native-results/paused.png /tmp/native-results/resumed.png null: 2>/tmp/native-results/resume-difference.txt; then
    echo 'Game display did not change after resume' >&2
    exit 1
else
    result=$?
    test "$result" = 1
fi
kill -0 "$gamepid"
echo 'Native Motif startup, Ernie play, pause freeze, and resume checks passed.'
echo 'Inspect /tmp/native-results screenshots for gameplay details.'
