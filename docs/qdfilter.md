qdfilter is used for calculating simple results from querydata such as daily maximum temperature, mean temperature and so on. For more complicated tasks the [SmartToolFilter] command is a more suitable choice.

## Command line

The command line syntax is

    qdfilter [options] startoffset endoffset function querydata

The querydata parameter can be either a file or a directory, in which case the newest file in the directory will be used. The result querydata is printed to standard output.

The startoffset and endoffset parameters specify the time interval offsets in minutes for which the actual results will be calculated. For example, there are 1440 minutes in a day, so using arguments -1440 0 would calculate daily results so that the result would be stored at the end time of the interval. Using 0 1440 would do the same, but the result would be stored at the start of the interval.

Note that if a negative offset is used as the first parameter, one must use a preceding "-" character on the command line to mark the end of options as in this example:

    qdfilter -p Precipitation1h - -660 0 sum filename.sqd > rsum.sqd

### Available functions

The function parameter specifies what is to be calculated.  
The possible choices are

* **min**  
for calculating the minimum value min(x)
* **max**  
for calculating the maximum value max(x)
* **mean**  
for calculating the mean value mean(x)
* **meanabs**  
for calculating the mean absolute value mean(abs(x))
* **sum**  
for calculating the sum sum(x)
* **median**  
    for calculating the median value median(x)
* **maxmean**  
    for calculating the value mean(mean(x),max(x))

### Available options

The available options are

* **-p param1,param2,...**  
    The parameters to be extracted as a comma separated list of parameter names, for example temperature,Precipitation1h. By default all parameters are extracted.
* **-l level1,level2,...**  
    The levels to be extracted as a comma separated list of level numbers. By default all levels are extracted.
* **-a**  
    Only the final time step will remain in the data. The startoffset is forced not to exceed the range of the data itself. For example, using startoffset -999999999 you should get the function value for the entire data, but with -1440 only for the final 24 hours
* **-t dt1,dt2,dt**  
    The time interval to be extracted as offsets from the origin time. For example parameters 24,48 would extract times between +24 and +48 hours from the origin time. dt1 may be omitted, in which case it is assumed to be zero. If the last dt parameter is given, it indicates the desired time step (in local time)
* **T dt1,dt2,dt**  
    Same as -t, but dt is used in UTC-time mode
* **-i hour**  
    The hour to be extracted (local time)
* **-I hour**  
    The hour to be extracted (UTC time)

## Examples

To calculate 06-18 UTC temperature maximum one can use

    qdfilter -p Temperature -I 18 - -720 0 max input.sqd > result.sqd

Note the use of the "-" character to mark the end of options so that the program will not try to interpret -720 as an option. Note that 720 minutes equals 12 hours.

To calculate 06-18 UTC precipitation sum one can use

    qdfilter -p Precipitation1h -I 18 - -660 0 sum input.sqd > result.sqd

Note the difference in the time interval when calculating sums. Since Precipitation1h represents 1 hour precipitation, using the argument 720 would include Precipitation1h at both ends of the 12 hour interval, which
would actually make the result a 13 hour precipitation sum. Using 660 minutes which 11 hours will make the result correct.

Also note that in both cases we used the -I option. Without it the results would have been calculated for all possible 12 hour intervals, not just those which end at 18 UTC.
