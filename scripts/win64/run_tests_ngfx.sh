rm /tmp/LOG_NGFX
TESTS=`cd tests && make tests-list | grep test-$FILTER | sed /rtt-feature/d` 
cd tests && TARGET_OS=Windows BACKEND=ngfx WSLENV=BACKEND/w make -k V=1 -j $NUM_THREADS $TESTS > /tmp/LOG_NGFX 2>&1; cd -
bash scripts/status.sh /tmp/LOG_NGFX
