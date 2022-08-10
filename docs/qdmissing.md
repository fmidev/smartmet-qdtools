The qdmissing command is used to check the amount of missing data in querydata. The command is mainly used in manual checks of production data when something has gone wrong, or in actual production scripts to prevent bad data from entering production.

## Command line

The command line syntax is

    qdmissing [options] querydata

The available options are

* **-h**  
    prints a brief help page on using qdmissing
* **-n**  
    analyze NaN values instead of the special missing value 32700. NaN is short for Not a Number, and can be a result for example from division by zero, or taking a square root of a negative number.
* **-t**  
    each timestep is analyzed separately.
* **-T [zone]**  
    specify the timezone for option -t, if not the system default.
* **-P [param1,param2,_.]**  
    the parameters to analyze, by default all parameters are analyzed. Note that the querydata option can also be a directory, in which case the newest file in the directory is analyzed.
* **-N**  
    print count instead of percentage
* **-w**  
    analyze all stations separately
* **-e limit**  
    stops running the program if there's more than limit missing values
* **-Z**  
    disable printing of results whose value is zero

## Examples

It is common for querydata containing observations to have missing values, for example because some stations do not measure all parameters, or do so at a longer interval than others. Here is a typical example for Finnish observation data:

    >qdmissing -t .
    200610100700 72
    200610100800 72
    200610100900 55
    200610101000 72
    200610101100 72
    200610101200 62
    200610101300 72
    200610101400 72
    200610101500 60
    200610101600 72
    200610101700 72
    200610101800 62
    200610101900 72
    ...

As can be seen from this example, there are less missing values for every third hour, meaning some stations do measurements only every third hour.

Here is an example on how bad querydata can be prevented from going into production in shell scripts:

    MISSING=`qdmissing $QUERYDATA`
    if [ _MISSING -gt 90 ]; then
       echo "Too much data is missing"
       exit 1
    fi
    # Proceed with normal production