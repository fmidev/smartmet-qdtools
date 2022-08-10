The qdsounding program is used for extracting point data from querydata which has multiple levels. The data could either be actual soundings or gridded forecasts from numerical model levels or pressure levels.

## Command line

The command line syntax is

    qdsounding [options] querydata

The input querydata can be either a file or a directory, in which case the newest file in the directory will be used. The results will be printed to standard output.

The available options are

* **-h**  
    print a brief summary of command line options
* **-P param1,param2,...**  
    the parameters to be printed out
* **-w wmo1,wmo2,...**  
    the stations to be printed out
* **-p loc1,loc2,...**  
    the names of the locations to be printed out, or pairs of coordinates as in lon1,lat1,lon2,lat2,...
* **-x lon**  
    the longitude
* **-y lat**  
    the latitude
* **-t zone**  
    the time zone.
* **-c filename**  
    the file with information on location coordinates
* **-z**  
    the level value will be printed after the ordinal number of the level. This option is mostly used for pressure level data.

The default value for the time zone is taken from file:

    /smartmet/cnf/smartmet.conf

from variable:

    qdpoint::timezone

Note that as opposed to [qdpoint] option -p is required, without it qdsounding will print an error message. This prevents the user from making assumptions on the order of the parameters in the querydata. qdpoint has no such restrictions for purely historical reasons - the dangers of making assumptions on the order of the data were discovered after qdpoint was in use but before qdsounding was implemented.
