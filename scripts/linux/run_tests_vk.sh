rm /tmp/LOG_VK
source nodegl-env/bin/activate
TESTS=`cd tests && make tests-list | grep test-$FILTER`
cd tests && BACKEND=vk make -j $NUM_THREADS $TESTS -k V=1 > /tmp/LOG_VK 2>&1; cd -
bash scripts/status.sh /tmp/LOG_VK

