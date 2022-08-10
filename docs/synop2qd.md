Reads all the files files given as arguments and tries to find SYNOP messages from them. All the found synops are combined in one output querydata file.

### Usage

    synop2qd [options] fileFilter1[,fileFilter2,...] > output

### Options

* **-s stationInfoFile**  
    File that contains different synop station infos.
* **-p <1001,SYNOP or 1002,SHIP or 1017,BUOY>**  
    Set producer id and name.
* **-t**  
    Put synops times to nearest synoptic times (=>3h resolution).
* **-v**  
    Verbose mode, reports more about errors encountered.
* **-S**  
    Use this to convert SHIP-messages to qd (not with B-option).
* **-B**  
    Use this to convert BUOY-messages to qd (not with S-option).

Note that fileFilter has to be put into quotes 

### Example

    # Do SYNOP stations
    synop2qd -t -s /smartmet/cnf/stations.csv "$INDIR/*" > $SYNOPFILE
     
    # Do SHIP SYNOP stations
    synop2qd -S -t -p 1002,SHIP "$INDIR/*" > $SHIPFILE
