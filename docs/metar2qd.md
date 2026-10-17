Program reads all the files/filefilter files given as arguments and tries to find METAR (and SPECI) messages from them. All the found synops are combined in one output querydata file.

### Usage

    metar2qd [options] fileFilter1[,fileFilter2,...] > output

### Options

* **-s station-info-file**  
    Give station info from in csv-format which is converted from file http://www.weathergraphics.com/identifiers/master-location-identifier-database.xls and which is provided now in file master-location-identifier-database.csv. The default is /usr/share/smartmet/stations.csv.
* **-v**  
    Use verbose warning and error reporting.
* **-F**  
    Ignore bad station descriptions in station info.
* **-r <round_time_in_minutes>**  
    Use messages time rounding, default value is 30 minutes.
* **-n**  
    Try reading NOAA METAR format files.
* **-W**  
    Don't create the combined Wind parameter in the result data.

### Example

    # Do METAR stations
    metar2qd -s /smartmet/cnf/stations.csv "$INDIR/*" > $METARFILE

Note that filemask should have quotes around it.
