The program can be used to generate data for SmartMet-editor. For example it combines winds to TotalWind-parameter and different weather parameters to WeatherAndCloudiness-parameter.

### Usage:

    Usage: qdversionchange [options] [qdVersion=7 keepCloudSymbol=1] < inputdata > outputData

### Options:

* **-t <0|1>**  
    Do TotalWind combinationParam, default = 1
* **-w <0|1>**  
    Do weatherAndCloudiness combinationParam, default = 1
* **-h**  
    Print help.
* **-g windGust-parId**  
    Add windGust param to totalWind using param with given parId.
* **-a**  
    Allow WeatherAndCloudinessParam to be created with minimum N and rr(1h|3h|6h) parameters.
* **-i** **<****filename****>**  
    Read input from given file instead of standard input.

Example usage:

    qdversionchange -a -t 1 0 7 < input > output
