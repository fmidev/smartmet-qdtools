**Note: gribt2toqd has been deprecated, please use gribtoqd, which handles both GRIB1 and GRIB2 instead.**

grib2toqd is a program that converts files from grib-format (version 1 and 2) to querydata-format. It is used when data is imported from external sources to SmartMet-system.

Before using grib2toqd user should know something about grib file structure. It is a bit different between grib1 and grib2. Grib files have data grids for each timestep parameter and level. Each parameter has number (different between grib1 and grib2), each level has level id and level value f.eg. 103  means fixed height level and value 2 means meter above mean sea level.
 
### Usage

    grib2toqd [options] inputgribfile|inputfilemask

### Options

**The Most Common options:**

* **-o outputfile**  
    By default data is writen to stdout.
* **-c paramChangeTableFile**  
    Mapping between grib parameter id and smartmet id are done in this configuration file. It is also possible to do some scaling f.eg. unit conversions. File should contain semicolon separated values GribID;SmartMetID;Name;Base;Scale;LevelType;LevelValue F.eg. to take 2m temperature in kelvins from grib2 file and to change it to celcius add:  0;4;T;-273.15;1;103;2 If level type and value are not specified, conversion is done to every level that is found in the data. Note that in grib2 files parameter number is category + parameter. F.eg. Relative Humidity Cat 1 and Param 1 = 1001.
* **-p producerId,producerName**  
    Set querydata producer id and name f.eg. 240,ECMWF Surface
* **-P** **<****projectionString****>**  
    Define projection that the data will be projected to. Use SmartMet projection format. Give also the grid size or default 50x50 will be used. E.g. FMI Scandinavia area is stereographic,20,90,60:6,51.3,49,70.2:82,91 (where 82,91 is used grid size). You can also set pressurelevel and hybridlevel output data grid sizes separately by adding them after first grid size e.g. after 82,91 you add ,50,40,35,25 and pressure data has 50x40 and hybrid datat has 35x25 grid sizes). You can also give three different projections for surface-, pressure- and hybrid-data. Give two or three projections separated by semicolons ';'. E.g. proj1:gridSize1[;proj2:gridSize2][;proj3:gridSize3]
* **-n**  
    Names output files by level type. E.g. output.sqd_levelType_100

**Other options:**

* **-m** **<****max-data-sizeMB****>**  
    Max size of generated data, default = 1024 MB
* **-l t105v3,t109v255,...***  
    Ignore following individual levels (e.g. 1. type 105, value 3, 2. type 109, value 255, etc.) You can use -wildcard with level value to ignore certain level type all together (e.g. t109v)
* **-L 105,109,...**  
    Accept only following level types.
* **-D** **<****definitions path****>**  
    Where is grib_api definitions files located, default uses path from settings (GRIB_DEFINITION_PATH=...).
* **-f don't-do-global-'fix'**  
    If data is in latlon projection and covers the globe 0->360, SmartMet cannot display data and data must be converted so its longitudes goes from -180 to 180. This option. will disable this function.
* **-t**  
    Reports run-time to the stderr at the end of execution
* **-v**  
    verbose mode
* **-d**  
    Crop all params except those mensioned in paramChangeTable (and their mensioned levels)
* **-g printed-grid-info-count**  
    If you want to print out (to cerr) info about different grids and projection information, give number of first grids here. If number is -1, every grids info is printed.
* **-G <x1,y1,x2,y2>**  
    Define the minimal subgrid to be cropped by the bottom left and top right longitude and latitude.
* **-r <specHumId,hybridRHId=13,hybridRHName=RH>**  
    Calculate and add relative humidity parameter to hybrid data. relative humidity parameter to hybrid data.
* **-H <sfcPresId,hybridPreId=1,hybridPreName=P>**  
    Calculate and add pressure parameter to hybrid data. Give surfacePressure-id, generated pressure id and name are optional. pressure parameter to hybrid data. Give surfacePressure-id, generated pressure idand name are optional.

### Grib definitions

The most important levels:

|ID   |Name   |Value|
|-----|-------|-----|   
100|isobaric level|pressure in hectopascals (hPa)
103|fixed height level|height above mean sea level (MSL) in meters
109|hybrid level|level number
200|entire atmosphere considered as a single layer|0

Full list of levels

#### Parameter change table

This configuration file contains semicolon separated table of smartmet and grib id:s. Also basic scaling can be done with base and scale columns. Finally specific level can be selected with LevelType and LevelValue columns. If you omit level information, changes will apply for every level. 

    #GribID;SmartMetID;Name;Base;Scale;LevelType;LevelValue
 
 
 
    # Pressure MSL
    151;1;P;0;0.01
 
    # Station Pressure SP
    134;472;SP;0;0.01
 
    # Geopotential Height GH
    156;2;Z
 
    # 2m Temperature 2T
    167;4;T;-273.15;1

To get grib id, you can use wgrib2, wgrib and grib_dump command line programs from NOAA and ECMWF. To know SmartMet id:s use ﻿qdinfo -l command.

Note that if you are converting grib2 files, parameter id in this file is actually category number + parameter number f.eg. Relative Humdity Cat 1 and Parameter 1 = 1001

Information about parameter numbers in grib1 and grib2 files, can be found in following links. But in the end you should use above mentioned tools to see what is actually in the data.

* [GRIB2 Product Table](http://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_table4-2.shtml)
* [GRIB1 Documentation](http://www.wmo.int/pages/prog/www/WDM/Guides/Guide-binary-2.html)

#### Projection

To define projection use information from SmartMet projection page. Additionally grid size is needed, to maintain specific resolution or to make resolution more coarse, use qdgridcalc to calculate grid size for certain resolution. 

    qdgridcalc stereographic,10,90,60:-13.8,31.8,73.2,55.6 25 25
    Grid size would be: 232 x 172
    Grid size would be at center of bottom edge: 21.9274 x 21.8816 km
    Grid size would be at center:           24.9983 x 24.9402 km
    Grid size would be at center of top edge: 27.0825 x 26.9947 km

Here we calculate what would be the grid size for this projection and area, if we want 15x15km resolution.

### Examples

    grib2toqd -H 472 -n -t -r 1000 -c $CNF/ecmwf.cnf -p "240,ECMWF Surface,ECMWF Pressure,ECMWF Hybrid" -P "stereographic,10,90,60:-13.8,31.8,73.2,55.6:232,172;stereographic,10,90,60:13.8,31.8,73.2,55.6:232,172;stereographic,10,90,60:5.5,51.1,45.0,60.1:86,64" -o /tmp/outname.sqd "/smartmet/data/incoming/L1D12140000*"

* -H gives gribId for surface pressure in gribfiles, this calculates pressure for each hybrid level
* -n names output files by level type, in this case outname.sqd_levelType_1, outname.sqd_levelType_100 and outname.sqd_levelType_109
* -t prints processing time for diagnostics purpose
* -r 1000 is gribId for specific humidity in order to calculate relative humidity for each hybrid level
* -c is the location of file that contains parameter id changes table
* -p "240,ECMWF Surface,ECMWF Pressure,ECMWF Hybrid" gives producer id and name pairs for each level
* -o /tmp/outputname.sqd is output filename
* "/smartmet/data/incoming/L1D12140000*"  is input file mask, please note mandatory quotes
