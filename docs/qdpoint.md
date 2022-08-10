The qpdoint command extracts the desired parameter values for the given location(s). The command is frequently used in production for example when plotting forecast graphs, rendering meteograms, printing data tables and so on.

See also: [qdpoint tutorial](qdpoint-tutorial.md)

## Command line

The command line syntax is

    qdpoint [options]

qdpoint recognizes location names which have been defined in

    /smartmet/share/coordinates/default.txt

An alternative coordinate file can be specified with option -c.

The available options are

* **-v**  
Verbose mode prints extra information at the start  
* **-q filename**  
The querydata to be used instead of the default one. If the argument is a directory, the newest file in it is used.  
* **-p place1,place2,...**  
The locations to be extracted identified by their names.  
* **-l filename**  
A file containing rows of form name,longitude,latitude. This is used when one wishes to extract information for so many places that using the command line alone would be cumbersome. The name can be an identifier for the location, including a number.  
* **-P param1,param2,...**  
The parameters to print out. Normally this option should be used, since one cannot trust in production scripts the order of the variables stored in the querydata to remain the same forever. Using the -P option forces an explicit order for the columns to be printed.  
* **-x longitude**  
The longitude of the location. Used with option -y.
* **-y latitude**  
The latitude of the location. Used with option -x.  
* **-w wmo1,wmo2,...**  
The numbers of the stations to be extracted.  
* **-c filename**  
An alternative file containing information on named locations.  
* **-n number_of_lines**  
Number of lines to print (newest lines).  
* **-N number_of_nearest_stations**  
How many stations to search for in the neighbourhood of the given point.  
* **-d max_search_radius_km**  
The maximum search radius for finding a station close enough to the given named point or coordinates. This option is not needed for gridded forecasts, but is sometimes needed for extracting observations in point data form.  
* **-C**  
The validity of the data for the nearest stations is checked.  
* **-f**  
Force lines with missing data to be also printed.  
* **-F**  
Only print timesteps in the future.  
* **-s**  
Also list the names and numbers of the stations being printed.  
* **-t zone**  
The timezone, as in America/Jamaica or UTC.  
* **-m string**  
The string to be printed for missing values. The default is "-"  
* **-i minutes**  
Interpolate values when a missing value is encountered. The argument specifies the maximum length of the gap for the interpolated result to be considered acceptably accurate. Depending on the parameter this value usually ranges from 180 up to 360.  
* **-u id**  
An optional ID so that one can identify which process started the call to qdpoint  

## Meta parameters

qdpoint recognizes some special parameters, whose value can be calculated when the time and location are known or provided some other parameters are present.

The available meta parameters are

* **MetaElevationAngle**  
The elevation angle of the sun at the location.  
* **MetaIsDark**  
0 or 1 depending on whether it is dark or not at the location.  
* **MetaWindChill**  
Wind chill as calculated from temperature and wind speed. The formula is not useful unless it is cold enough (below 5°C). MetaWindChill is calculated in the following way:  


  

       float windChill(temp, wind)
       {
         // 1.7 m/s = walking speed
         float w = max(1.7f,wind);
         double wpow = std::pow(static_cast<double>(w),0.16);
         return static_cast<float>(13.12+0.6215*temp-13.956*wpow+0.4867*temp*wpow);
       }




* **MetaMoonIlluminatedFraction**  
The fraction of the moon that is illuminated by the sun.


