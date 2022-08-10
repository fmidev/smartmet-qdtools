* [Producer names](#producer-names)
  * [Known producers](#known-producers)
  * [Inspecting the producer](#inspecting-the-producer)
  * [Changing the producer](#changing-the-producer)
  * [Setting the producer in conversion programs](#setting-the-producer-in-conversion-programs)

## Producer names

The Smartmet editor usually distinguishes various data sources from the producer name and/or number stored inside the querydata file.

### Known producers

Additional external resources: GRIB1 level types

Additional internal resources: Wikise

|Number   |Name   |Comment   |  
|---------|-------|----------|
0|UNKNOWN|Used when the producer is unknown
1|HIRLAM|RCR-HIRLAM
2|HIRMESO|MBE-HIRLAM
11|TAFHIR|TAFHIR
54|GFS|GFS forecast
101|ANALYSIS|Kriging etc
102|TIESAA|FMI road condition forecast
104|MESAN|Mesoscale analysis
105|MTLICE|Ice analysis
107|LAPF|LAPS analysis for Finland
108|SNOWLOAD|Snow load in trees
109|LAPSSCAN|LAPS analysis for Scandinavia
111|WAM_MBE|MBE-based wave forecast
112|WAM_EC|EC-based wave forecast
114|OAAS_MBE|MBE-based OAAS-model
116|WALK_NOIC|Pedestrian model, no ice storage
117|WALK_WIC|Pedestrian model with ice storage
118|ECKALLAPS|Kalman filtered EC forecast
120|EGRR_VAAC|MetOffice volcano advisory
131|ECMWF|Operational ECMWF
132|ECGSEF|ECMWF fine mesh marine model
160|SmhiMesan|Mesoscale analysis by SMHI
199|AROME|Finnish Arome
210|AROMTA|Arome postprocessed fields
220|MTAMBE|MBE-HIRLAM, postprocessed fields
230|MTAHIRLAM|MTA-HIRLAM, postprocessed fields
240|MTAECMWF|MTA-ECMWF, postprocessed fields
242|ECM_PROB|ECMWF EPS probabilities
1001|SYNOP|Synoptic observations
1002|SHIP|Observations by ships
1004|AWS|
1005|TEMP|TEMP sounding observations
1006|RAWTEMP|RAW-TEMP sounding observations (used in Smartmet editor)
1012|FlashObs|Flash detector
1013|IRIS|Radar software
1014|NRD|NORDRAD
1015|AIREP|Airplane observations
1017|BUOY|Buoy observations
1021|SIGMET|SIGMET
1023|RoadObs|Road stations
1025|METAR|METAR message
1026|SAFIR|SAFIR
1032|AERO|Soundings
1101|TestBed|Helsinki Testbed VXT-data
2001|METEOR|Meteorologist
2004|AUTOMATIC|Automatic (derived) values
2011|TAFMET|TAF-MET

### Inspecting the producer

The producer can be inspected outside the editor using the qdinfo command:

    /smartmet/data/hirlam/eurooppa/pinta/querydata >qdinfo -r -q .
    Information on the querydata producer:
    id  = 230name   = HIRLAM pinta

### Changing the producer

If necessary, the producer of an existing querydata file can be changed using the qdset command:

    > qdset
    Error: 2 parameter expected, dataFile 'parameter-id/name'
     
    Usage: qdset [options] dataFile param(Id/name e.g. 4 or Temperature)
 
    Options (default values are allways current parameters current values):
 
        -n paramName    New parameter name
        -d <paramId>  New parameter id (see FmiParameterName)
        -N producerName New producer name (changes all parameters prod name,
             no param id argument needed
        -D <producerId>   New producer id (changes all parameters prod ID,
             no param id argument needed
            ....
 
        Example usage: changeParam -n 'Temperature' dataFile Temperature

### Setting the producer in conversion programs

Many of the programs which convert data to querydata format support setting the simultaneously. For example, h5toqd supports the following options:

    -p [ --producer ] arg   producer number,name
    --producernumber arg    producer number
    --producername arg      producer name

