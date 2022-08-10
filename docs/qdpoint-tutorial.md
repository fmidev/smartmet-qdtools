The qpdoint command extracts the desired parameter values for the given location(s). The command is frequently used in production for example when plotting forecast graphs, rendering meteograms, printing data tables and so on.

See also qdpoint [manual](qdpoint.md).

Table of contents:

* [Query Grid Data Based on Place Name](#query-grid-data-based-on-place-name)
* [Query Grid Data Based on Coordinates](#query-grid-data-based-on-coordinates)
* [Query Point Data Based on WMO Number](#query-point-data-based-on-wmo-number)

## Query Grid Data Based on Place Name

Getting temperature,wind speed, wind direction and overall weather in Riga happens with following command:

    qdpoint -P Temperature,WindSpeedMS,WindDirection,WeatherSymbol3 -p Riga -q /smartmet/data/ecmwf/europe/surface/querydata

Note that -q option does not require the actual querydata filename. If it is omitted, the newest querydata file in the directory is used.

The output is following

    201110250300 3.7 3.1 144.8 1.0
    201110250600 2.8 2.9 129.6 1.0
    [...]
    201111032100 7.7 4.5 180.0 1.0
    201111040300 5.1 4.4 200.0 1.0

Where the first column is times tamp in format year month day hour minute and following columns are asked values in the same order they are asked.

## Query Grid Data Based on Coordinates

If a place name is not known or it is not recognized, the data can be fetched with coordinate information following:

    qdpoint -P Temperature,WindSpeedMS,WindDirection,WeatherSymbol3 -x 24.1 -y 56.9  -q /smartmet/data/ecmwf/europe/surface/querydata

## Query Point Data Based on WMO Number

When querying observations, wmo numbers need to be used. For example:

    qdpoint -P Temperature,WindSpeedMS,WindDirection,WeatherSymbol3 -w 26503 -q /smartmet/data/nmc/aws/hourly/querydata

The output is similar, except that WeatherSymbol3 is missing since it is not observed.
