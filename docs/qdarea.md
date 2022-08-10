qdarea is a program for extracting weather information over user defined areas in querydata. The program can be used for example for calculating daily maximum temperatures at some point, within some radius around a point, or even inside generic polygon.

See also: [qdarea tutorial](qdarea-tutorial.md)

## Command line

The command line syntax is

     qdarea [options]

The results are printed to standard output.

The available options are

* **-P param1,param2,...**  
List of results to calculate, for example 'mean(min(t2m)),mean(max(t2m)'  
* **-p location1::location2,...**  
For example Helsinki:25 or jamaica.svg
* **-T interval**  
The time interval, for example 06-18 for days or 6:12 for nights too
* **-t timezone**  
For example America/Jamaica or UTC, the default is specified in /smartmet/cnf/smartmet.conf
* **-q querydata**
The querydata
* **-c coordinatefile**  
The default coordinatefile is specified in /smartmet/cnf/smartmet.conf
* **-s**  
Print results as a PHP hash table
* **-S name1,name2,...**  
Print results as a PHP hash table with named data fields
* **-E**  
Print times in Epoch seconds
* **-v**  
Verbose mode  

### Option -P

The parameters given for option -P are of the form

    areafunction(timefunction(parametername))

If the area and time functions are the same, the form can be simplified to

    function(parametername)

The simpler form can also be used when the location is a point, not an area.  

**Known parameter names**

qdarea does not recognize all parameters that can be stored in querydata.  
Instead only the following parameters are recognized:


|Name       |Aliases    |
|-----------|-----------|
|Temperature|t2m,t|
|Precipitation|rr1h|
|PrecipitationType|rtype|
|PrecipitationForm|rform|
|PrecipitationProbability|pop|
|Cloudiness|n|
|Frost|-|
|SevereFrost|-|
|RelativeHumidity|-|
|WindSpeed|wspd,ff|
|WindDirection|wdir,dd|
|Thunder|pot|
|RoadTemperature|troad|
|RoadCondition|wroad|
|WaveHeight|-|
|WaveDirection|-|
|ForestFireWarning|mpi|
|Evaporation|evap|
|DewPoint|tdew|
|HourlyMaximumGust|gust|
|FogIntensity|fog|

**Functions**

The functions available for option -P are

|Function    |Description    |
|------------|---------------|
|mean|Mean value|
|max|Maximum value|
|min|Minimum value|
|sum|Sum|
|sdev|Standard deviation|
|trend|Trend|
|change|Total change|
|count[lo:hi]|Occurrance count, requires limits|
|percentage[lo:hi]|Occurrance percentage, requires limits|

If either limit is missing from count or percentage, the accepted range is open ended.

### Option -p

The argument for option -p can be in one of the following forms

* **location**  
for example Helsinki
* **location:radius**  
for example Helsinki:50 for a 50 km radius circle around Helsinki
* **lon,lat**  
for example 25,60 for the approximate location of Helsinki
* **lon,lat:radius**  
for adding a radius around the coordinate
* **areaname**
the filename containing an SVG path defining the area
* **areaname:radius**  
the filename containing an SVG path defining the area plus a radius around it  

One can obtain results for multiple areas simultaneously by separating the location definitions with the character sequence "::", as in London::Paris.

### Option -T

Option -T is used for defining the time interval over which
the results will be calculated. The available forms for the argument
to the option are

* **data**
the native timesteps of the data
* **all**  
the full range covered by the data
* **starthour-endhour**  
A simple time interval, for example 06-18 for days and 18-06 for nights.
* **starthour-endhour:maxstarthour-minendhour**  
In this form we define a minimum time interval in case there is too little data to cover the entire interval.
For example, using 06-18:10-18 we could get results for a day even if the data starts after 6 am.
* **interval**  
The weather data is divided into equal length intervals. The interval must divide 24 evenly, or be a multiple of 24.
The value 0 is interpreter to mean hourly values so that there will be no time integration of the results at all.
* **starthour:interval**  
In this form we can shift the start time of the intervals. For example, using 3:6 we would get 6 hour intervals 3-9, 9-15, 15-21 and 21-3.
* **starthour:interval:mininterval**  
In this form we can also specify a minimum requirement for the interval in case there is lack of data either at the start or the end of the weather data.  

## Examples

To get day and night minimum and maximum temperatures and precipitation sums for Helsinki area one could use

    qdarea -T 12 -p Helsinki:25 -P 'mean(min(t2m)),mean(max(t2m)),mean(sum(rr1h))'

The output looks like this

    Helsinki:25 200610131200 200610140000 8.2 10.9 0.0
    Helsinki:25 200610140000 200610141200 4.8 10.3 0.0
    Helsinki:25 200610141200 200610150000 9.7 11.0 1.0
    Helsinki:25 200610150000 200610151200 6.5 9.7 0.0
    Helsinki:25 200610151200 200610160000 5.8 8.4 0.0
    Helsinki:25 200610160000 200610161200 2.6 7.1 0.0
    Helsinki:25 200610161200 200610170000 7.1 10.5 0.0
    Helsinki:25 200610170000 200610171200 8.6 10.6 0.0
    Helsinki:25 200610171200 200610180000 9.1 12.3 0.0
    Helsinki:25 200610180000 200610181200 8.3 9.1 0.0
    Helsinki:25 200610181200 200610190000 4.0 9.1 0.0
    Helsinki:25 200610190000 200610191200 2.9 4.0 0.0
    Helsinki:25 200610191200 200610200000 1.5 3.1 0.4
    Helsinki:25 200610200000 200610201200 0.7 1.8 1.7
    Helsinki:25 200610201200 200610210000 0.3 2.9 0.3

The daily mean maximum wind speed for Bay of Bothnia is calculated with

    qdarea -T 24 -p bothnia -P 'mean(max(wspd))'

where bothnia.svg contains the SVG-path of the area. The output looks like this

    bothnia 200610140000 200610150000 8.9
    bothnia 200610150000 200610160000 10.0
    bothnia 200610160000 200610170000 10.4
    bothnia 200610170000 200610180000 8.6
    bothnia 200610180000 200610190000 10.7
    bothnia 200610190000 200610200000 9.1
    bothnia 200610200000 200610210000 6.8

The six-hour interval mean winds in UTC-time are

    qdarea -t UTC -T 3:6 -p merialueet/B1 -P 'mean(wspd),mean(wdir),sdev(wdir)'

Note that we can use the standard deviation of the wind direction to estimate whether the wind direction is variable or not.  
The output looks like this:

    bothnia 200610130900 200610131500 4.2 271.5
    bothnia 200610131500 200610132100 4.3 267.7
    bothnia 200610132100 200610140300 6.3 263.1
    bothnia 200610140300 200610140900 6.0 281.7
    bothnia 200610140900 200610141500 6.1 319.7
    bothnia 200610141500 200610142100 7.8 315.0
    bothnia 200610142100 200610150300 9.3 342.1
    bothnia 200610150300 200610150900 6.2 13.3
    bothnia 200610150900 200610151500 2.3 315.2
    bothnia 200610151500 200610152100 6.6 193.1
    bothnia 200610152100 200610160300 9.9 215.9
    bothnia 200610160300 200610160900 9.5 242.4
    bothnia 200610160900 200610161500 8.7 262.8
    bothnia 200610161500 200610162100 8.6 284.1
    bothnia 200610162100 200610170300 8.4 295.9

Note that the intervals are 03-09, 09-15, 15-21, 21-03. These are the intervals customarily identified by times 06, 12, 18 and 00 respectively.

Finally, to calculate the percentage of rainy hours each day one could use

    qdarea -T 24 -p Helsinki -P 'mean(percentage[0.1:](rr1h))'

Note that we consider an hour rainy if the precipitation amount is at least 0.1 millimeters.  
The output looks like this:

    Helsinki 200610140000 200610150000 24.0
    Helsinki 200610150000 200610160000 0.0
    Helsinki 200610160000 200610170000 0.0
    Helsinki 200610170000 200610180000 0.0
    Helsinki 200610180000 200610190000 0.0
    Helsinki 200610190000 200610200000 8.0
    Helsinki 200610200000 200610210000 52.0

 
## How to create area svg's from shapefiles

    cp /smartmet/share/gis/shapes/natural_earth/50m_cultural/*countries* shapes
  
    ogrinfo 50m_admin_0_countries.shp 50m_admin_0_countries |grep -ri colombia
  
    shapefilter -f NAME="Colombia" 50m_admin_0_countries colombia
  
    shape2svg colombia
  
    qdarea -v -p Colombia.svg:10 -T 6 -P 'max(wspd),mean(wdir),mean(t2m)' -q /smartmet/data/meteor/colombia/surface/querydata/

 

 
