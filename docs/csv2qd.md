The csv2qd program converts ASCII files in CSV format into querydata format. The CSV format is documented in [Wikipedia](http://en.wikipedia.org/wiki/Comma-separated_values)

### Usage

    csv2qd [options] infile outfile

### Options

* **-h [--help]**  
    print out help message
* **-v [--verbose]**  
    set verbose mode on
* **-V [--version]**  
    display version number
* **-m [--missing] arg**  
    missing value string (default is empty string)
* **--prodnum arg**  
    producer number (default=1001)
* **--prodname arg**  
    producer name (default=SYNOP)
* **-i [--infile] arg**  
    input csv file
* **-o [--outfile] arg**  
    output querydata file
* **-p [--params] arg**  
    parameter names of csv columns
* **-P [ --paramsfile] arg**  
    parameter configuration file (/smartmet/share/csv/parameters.csv)
* **-O [--order] arg**  
    column ordering (idtime|timeid|idtimelevel|idleveltime|timeidlevel|timelevelid|levelidtime|leveltimeid)
* **-S [ --stationsfile ] arg**  
    station configuration file (/smartmet/share/csv/stations.csv)

### Input file syntax

When vertical level information is not included there are two possibilities

    station_id,timestamp,value1,value2,value3,...
 
    timestamp,station_id,value1,value2,value3...

The first format is chosen with "-O idtime", the second with "-O timeid".

If level information is included, there are 6 possible combinations:

    station_id,timestamp,level
    station_id,level,timestamp
    ...
    level,timestamp,station_id

The order is chosen with option -O.

### The order of the columns in the input CSV file

The order of the first 2-3 columns, depending on whether level information is present or not, is determined by the -O option. The order of the remaining columns (the actual parameter values) is determined by option -p. The parameter names are separated with comma, as in

    csv2qd -p Temperature,Precipitation1h ...

### Timestamp syntax

The time stamp must be given in UTC time. The program accepts several input forms:

*    ISO-format: 20110311T131920
*    Timestamp format: 201103111319
*    Epoch seconds since 1970: 1299827871
*    SQL-format: 2011-03-11 13:19:20
*    XML-format: 2011-03-11T13:19:20 or 2011-03-11T13:19:20.12345Z

### Location file syntax

The locations are identified in the input by the ID of the stations. The ID can be any string, usually one uses the numeric station ID or the name of the stations.

The stations themselves must be described in a separate CSV file, which can be specified with option -S. The format of the file is

    station_id,station_number,longitude,latitude,name

A sample file:

    1001,1001,-8.66,70.93,"Jan Mayen"
    98747,98747,124.28,8.43,"Lumbia Airport"
    3318,3318,-3.03,53.76,"Blackpool Airport"

### Parameter defininition file syntax

The parameters are described using a CSV file which can be specified with option -P. The format of the file is

    parameter_number,name,minimum_value,maximum_value,interpolation_method,precision

32700 is a special value which indicates the minimum or maximum is not set.

Interpolation can be of types

*    linear, which is used for most continuous variables such as temperature
*    nearest, which is used for enumerated variables such as precipitation form

Precision is given using the C-language printf syntax.

A sample file:

    4,Temperature,-300,1000,linear,%.1f
    20,WindDirection,0,360,linear,%.1f
    353,Precipitation1h,0,1000,linear,%.1f
    57,PrecipitationForm,32700,32700,nearest,%.0f
    13,Humidity,0,110,linear,%.1f

Note that it is possible to get humidity measurements slightly over 100% from observation stations.
