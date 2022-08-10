Table of Contents
* [List of querydata handling tools](#list-of-querydata-handling-tools)
  * [Most important ones](#most-important-ones)
  * [Less important ones](#less-important-ones)
  * [Qdtools tutorials](#qdtools-tutorials)
* [List of data conversion tools](#list-of-data-conversion-tools)
* [List of shapefile handling tools](#list-of-shapefile-handling-tools)
* [Other tools](#other-tools)
* [Other Information](#other-information)
* [How to call the command line tools from scripts](#how-to-call-the-command-line-tools-from-scripts)

## List of querydata handling tools

### Most important ones:
  
* [qdinfo](docs/qdinfo.md) for displaying basic information about the data  
* [qdstat](docs/qdstat.md) for calculating statistics on data values  
* [qdpoint](docs/qdpoint.md) for displaying a timeseries for selected points  
* [qdarea](docs/qdarea.md) for displaying timeseries for selected areas  

### Less important ones:

* [qdcrop](docs/qdcrop.md) for extracting parts of querydata  
* [qdfilter](docs/qdfilter.md) for calculating new results from the data  
* [qdscript](docs/qdscript.md) for executing SmartTools-macros used by SmartMet editor  
* [combineHistory](docs/combineHistory.md) for combining several querydata files with respect to time  
* [qdinterpolatearea](docs/qdinterpolatearea.md) for changing the projection of querydata  
* [qdcheck](docs/qdcheck.md)  
* [qddiff](docs/qddiff.md) for analyzing differences between querydata
* [qdmissing](docs/qdmissing.md)
* [qdproject](docs/qdproject.md) for performing projection calculations
* [qdsounding](docs/qdsounding.md) for extracting soundings from querydata
* [qdsplit](docs/qdsplit.md) for splitting a querydata into separate files
* [qdinterpolatetime](docs/qdinterpolatetime.md) for interpolating querydata to another time resolution
* [qdcombine](docs/qdcombine.md) for combining all querydata-files from some directory to one querydata-file
* [qdview](docs/qdview.md) for showing data coverage as a map
* [qdset](docs/qdset.md) for changing querydata meta information (i.e. parameter names)
* [qdgridcalc](docs/qdgridcalc.md) to calculate grid size for given projection area
* [qdsmoother](docs/qdsmoother.md) to smoothen querydata to get nicer animations

### Qdtools tutorials

* [**qdinfo tutorial**](docs/qdinfo-tutorial.md)  
    The tutorial demonstrates how to investigate data coverage, containing parameters etc.
* [**qdpoint tutorial**](docs/qdpoint-tutorial.md)  
    The tutorial demonstrates how to retrieve data to the point.
* [**qdarea tutorial**](docs/qdarea-tutorial.md)  
    The simple tutorial shows how to retrieve maximum wind and mean temperature from the lake Usmas ezers. First, the tutorial shows how to crop lake from larger shape file. Then the cropped shape is converted to SVG path, which is finally used by qdarea to retrieve data.
* [**qdscript tutorial**](docs/qdscript-tutorial.md)  
    The tutorial shows one example how to process querydata to a fictional sailing index data. The tutorial shows how to investigate investigate required information from querydata, how to create and run Smarttool-scripts from command line, how to crop certain parameters from querydata and how to change parameter names.

## List of data conversion tools

* [gribtoqd](docs/gribtoqd.md) converts GRIB1 and GRIB2 files to querydata
* [grib2toqd](docs/grib2toqd.md) converts GRIB2-files to querydata **(deprecated, please use gribtoqd instead)**
* [qdtogrib](docs/qdtogrib.md) converts querydata-file to grib-file
* [ashtoqd](docs/ashtoqd.md) converts MetOffice CSV ash advisories to querydata.
* [bufrtoqd](docs/bufrtoqd.md) converts BUFR observations to querydata.
* [radartoqd](docs/radartoqd.md) converts radar data in OPERA BUFR form to querydata
* [combinepgms2qd](docs/combinepgms2qd.md) converts several RADAR formatted pgm files into single querydata
* [csv2qd](docs/csv2qd.md) for generating querydata from ASCII data
* [qd2csv](docs/qd2csv.md) converts querydata to csv
* [flash2qd](docs/flash2qd.md) convert flash text files to querydata
* [grib2tojpg](docs/grib2tojpg.md) draws jpeg-image from grib2-file
* [h5toqd](docs/h5toqd.md) converts EUMETNET OPERA radar files to querydata
* [laps2qd](docs/laps2qd.md) convert LAPS-data to querydata
* [metar2qd](docs/metar2qd.md) find METAR message form data and converts it to querydata
* [nctoqd](docs/nctoqd.md) converts CF-1.4 conforming NetCDF to querydata
* [wrftoqd](docs/wrftoqd.md) converts WRF NetCDF to querydata
* [pgm2qd](docs/pgm2qd.md) converts RADAR formatted pgm files into querydata
* [qdversionchange](docs/qdversionchange.md)
* [synop2qd](docs/synop2qd.md) find SYNOP message from data and converts it to querydata
* [temp2qd](docs/temp2qd.md) find TEMP sounding message from data and converts it to querydata

## List of shapefile handling tools

For handling shapefiles, two external programs are very handy:

*  [QGis](http://www.qgis.org/) desktop tool for showing and modifying shapes
*  [ogr2ogr](http://www.gdal.org/ogr2ogr.html) to change shape projections

Smartmet tools:

* [shape2ps](docs/shape2ps.md) for generating an image out of shapefiles
* [shapefilter](docs/shapefilter.md) for extracting parts of a shapefile
* [shape2svg](docs/shape2svg.md) for creating textgen region paths
* [shape2xml](docs/shape2xml.md) for browsing shape data

## Other tools

* [cleaner](docs/cleaner.md) for removing aged files from the system
* [compositealpha](docs/compositealpha.md) for adding transparency to a plain RGB image

## Other information

* [Projection descriptions in SmartMet](docs/projection-descriptions-in-SmartMet.md)
* [Querydata producer names](docs/querydata-producer-names.md)
* [Server directory structure](docs/server-directory-structure.md)
* [Executing script](docs/executing-script.md)
* [qdcontour](docs/qdcontour.md) legacy querydata rendering program for drawing animations.

## How to call the command line tools from scripts

The most common scripting languages used in production are:

1. Bash shell scripts
2. PHP
3. Perl.

Most of the tools handling massive amounts of weather data are written in C++. Many of the tools are named qdsomething to remind the user one is processing querydata.

In bash scripts, using the C++ tools is very straight forward: one simply uses the command in the script as one would do it in the command line. Here is how one would direct temperature forecasts into a a file: 

    qdpoint -p Arima -P Temperature > results.txt

In PHP and Perl, the situation is different. One must use the system command, or one of its siblings, like the PHP backquotes ``. The previous example would be done like this: 

    system("qdpoint -p Arima -P Temperature > results.txt")

However, it is more typical to store the result directly into a variable like this (in PHP): 

    $forecast = `qdpoint -p Arima -P Temperature`;
