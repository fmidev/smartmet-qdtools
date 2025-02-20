#! /bin/sh

# Simple script to prepare the results for smartmet-qdtools-test-data update

if ! test -f /usr/include/eccodes_version.h ; then
    echo /usr/include/eccodes_version.h not found
    exit 1
fi

eccodes_version_major=$(awk '$1 == "#define" && $2 == "ECCODES_MAJOR_VERSION" { print $3 }' /usr/include/eccodes_version.h)
eccodes_version_minor=$(awk '$1 == "#define" && $2 == "ECCODES_MINOR_VERSION" { print $3 }' /usr/include/eccodes_version.h)
echo $eccodes_version_major
echo $eccodes_version_minor

if test "$eccodes_version_major" != "2" ; then
    echo "Unsupported eccodes major version $eccodes_version_major"
    exit 1
fi

for file in $(find results/gribtoqd -name '*.sqd.tmp'); do
    if ! test -d gribtoqd_results_update ; then
        mkdir gribtoqd_results_update;
    fi
    out="gribtoqd_results_update/$(basename $file | perl -pe 's/(?:\.tmp)?(?:\.\d+)?\.sqd\..*$//').$eccodes_version_minor.sqd.xz"
    echo "*** processing $file and writting $out"
    cat $file | xz -9vv >$out
done
