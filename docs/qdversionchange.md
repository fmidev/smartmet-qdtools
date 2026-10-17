The program can be used to generate data for SmartMet-editor. For example it combines winds to TotalWind-parameter and different weather parameters to WeatherAndCloudiness-parameter.

### Usage:

    Usage: qdversionchange [options] [qdVersion=7 keepCloudSymbol=0] < inputdata > outputData

### Options:

* **-t <0|1>**  
    Do TotalWind combinationParam, default = 1
* **-w <0|1>**  
    Do weatherAndCloudiness combinationParam, default = 1
* **-h**  
    Print help.
* **-g windGust-parId**  
    Add windGust param to totalWind using param with given parId.
* **-f precipForm-parId-list <id1[,id2,...]>**  
    Add precipForm param values to Weather using the given param(s).
* **-F fog-parId-list <id1[,id2,...]>**  
    Add fog param values to Weather using the given param(s).
* **-P pot-parId-list <id1[,id2,...]>**  
    Add pot param (probability of thunder) values to Weather using the given param(s). For options -f, -F and -P a positive parId means the param will be removed from the result data, a negative parId means the param will be left in the result data.
* **-a**  
    Allow WeatherAndCloudinessParam to be created with minimum N and rr(1h|3h|6h) parameters.
* **-i** **<****filename****>**  
    Read input from given file instead of standard input. Enables memory mapping of the input.
* **-m <thread-count>**  
    Max used worker threads, default all.
* **-p**  
    Add accurate precip param and calculate snowFall param per 1h.
* **-b**  
    Build time bag instead of time list if possible (required by TAF editor).
* **-N**  
    Convert (observed) N parameter from octas to %, default = not.

Example usage:

    qdversionchange -a -t 1 -w 0 7 < input > output
