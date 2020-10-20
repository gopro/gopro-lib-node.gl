LOG_FILE=$1
passed=`cat $LOG_FILE | sed 's/\r$//' | grep -e 'passed$'`
tests=`cd tests && make tests-list | grep -e '^test-'`

function status {
    num_tests=` echo "$tests"  | grep -e "^test-$1-" | wc -l`
    num_passed=`echo "$passed" | grep -e "^$1_" | wc -l`
    percent_passed=`echo "$num_passed / $num_tests * 100" | bc -l`
    printf "%-10s: %3d out of %3d passed ( %3.0f%% )\n" $1 $num_passed $num_tests $percent_passed
}

test_names="api anim blending compute data live media rtt shape text texture transform "

for test_name in $test_names; do status $test_name; done

total_tests=`echo "$tests" | wc -l`
total_passed=`echo "$passed" | wc -l`
percent_passed=`echo "$total_passed / $total_tests * 100" | bc -l`
printf "%-10s: %3d out of %3d passed ( %3.0f%% )\n" "Total" $total_passed $total_tests $percent_passed
