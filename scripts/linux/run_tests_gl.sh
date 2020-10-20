rm /tmp/LOG_GL
source nodegl-env/bin/activate
TESTS=`cd tests && make tests-list | grep test-$FILTER`
cd tests && BACKEND=gl make -j $NUM_THREADS $TESTS -k V=1 > /tmp/LOG_GL 2>&1; cd -
bash scripts/status.sh /tmp/LOG_GL
