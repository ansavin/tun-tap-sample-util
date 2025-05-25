nohup ./util $1 $2 &
PID=$!
echo $PID > /tmp/tun-tap-sample-util.pid