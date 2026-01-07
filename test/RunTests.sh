#! /bin/sh

TESTS=$(find . -maxdepth 1 -a -type f -a -name '*.test' | sed -e 's:^\./::')

XFAIL="
    qdfilter.test
    radartoqd.test
"

echo "Running tests"

rm -f *.err *.xpass *.xfail
for prog in $TESTS; do
    echo ''
    echo $prog
    if ./$prog ; then
        if echo $XFAIL | grep -q $prog; then
            touch $prog.xpass
        fi
    else
        if echo $XFAIL | grep -q $prog; then
            touch $prog.xfail
        else
            touch $prog.err
        fi
    fi
done

xpass_list=$(find . -maxdepth 1 -a -type f -a -name '*.xpass' | sed -e 's:^\./::')
xfail_list=$(find . -maxdepth 1 -a -type f -a -name '*.xfail' | sed -e 's:^\./::')
fail_list=$(find . -maxdepth 1 -a -type f -a -name '*.err' | sed -e 's:^\./::')

ok=true
if ! [ -z "$xpass_list" ] ; then
    echo ''
    echo '###### Following tests were expected to fail but did not'
    for prog in $xpass_list; do
        echo "    $prog"
    done
    echo ''
    ok=false
fi

if ! [ -z "$xfail_list" ] ; then
    echo ''
    echo '###### Following tests failed as expected'
    for prog in $xfail_list; do
        echo "    $prog"
    done
    echo ''
fi

if ! [ -z "$fail_list" ] ; then
    echo ''
    echo '###### Following tests were failed unexpectedly'
    for prog in $fail_list; do
        echo "    $prog"
    done
    ok=false
    echo ''
fi

$ok
