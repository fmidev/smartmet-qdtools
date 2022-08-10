qddiff is a simple program for analyzing differences between two querydata. The command is used in routines for example for analyzing which parameters and timesteps have changed in the forecasts so that the routine can decide which images have to be rendered again. The CPU savings in a very large production system can be very significant if only a small part of the forecast has been changed.

## Command line

The command line syntax is

    qddiff [options] querydatafile1 querydatafile2 outputfile
    qddiff [options] querydatadir outputfile

If the first parameter is a directory, the two newest files
in it are compared.

The available options are:

* **-h**  
    prints information on command line options
* **-t**  
    only new timesteps in file are saved to outputfile
* **-p**  
    only only common times where parameters have changed
* **-v**  
    verbose mode
* **-V**  
    verbose analysis of the differences in the files
* **-d**  
    debug mode (verbose mode goes automatically on) Note that if one does not wish to save the differences between the querydata, for example if only the results if option -V are desired, one can use /dev/null as the outputfile.

## Examples

Here is an analysis of what the meteorologist did at Finnish Meteorological Institute when the Scandinavian forecast was edited the last time:

    > pwd
    /data/pal/querydata/pal/skandinavia15km/pinta_xh
    > qddiff -V . /dev/null
    0 times were missing from data 1
    0 parameters were missing from data 1
    0 levels were missing from data 1
    11 timesteps were unchanged
    56 timesteps were changed
    unchanged Pressure
    unchanged Temperature
    unchanged DewPoint
    unchanged Humidity
    unchanged WindDirection
    unchanged WindSpeedMS
    unchanged WindVectorMS
    unchanged WindUMS
    unchanged WindVMS
    CHANGED PrecipitationType in 10 timesteps
    CHANGED PrecipitationForm in 10 timesteps
    CHANGED TotalCloudCover in 25 timesteps
    unchanged KIndex
    unchanged PoP
    unchanged ProbabilityThunderstorm
    CHANGED MiddleAndLowCloudCover in 56 timesteps
    CHANGED LowCloudCover in 56 timesteps
    CHANGED MediumCloudCover in 1 timesteps
    unchanged HighCloudCover
    CHANGED RadiationLW in 25 timesteps
    CHANGED RadiationGlobal in 19 timesteps
    unchanged FogIntensity
    CHANGED WeatherSymbol1 in 10 timesteps
    CHANGED WeatherSymbol3 in 26 timesteps
    CHANGED Precipitation1h in 10 timesteps
    unchanged HourlyMaximumGust

From the analysis one can see the meteorologist has edited mainly precipitation and cloudiness, not for example wind or temperature.
