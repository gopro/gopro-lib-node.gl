rm /tmp/LOG_NGFX
source nodegl-env/bin/activate
TESTS=`cd tests && make tests-list | grep test-$FILTER`
cd tests && BACKEND=ngfx make -k V=1 -j $NUM_THREADS $TESTS > /tmp/LOG_NGFX 2>&1; cd -
bash scripts/status.sh /tmp/LOG_NGFX
