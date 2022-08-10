The qdinfo program prints out information on the given querydata. The user can either select the subset of information to be printed, such as the locations in the querydata, or print all of them.

qdinfo is not usually used in production scripts, but it can be provided the output is processed using a programming language
to extract the desired words from the output.

See also qdinfo [manual](qdinfo.md).

Table of contents:

* [Investigating Stored Parameters](#investigating-stored-parameters)
* [Investigating Time Information](#investigating-time-information)
* [Investigating All Meta Data](#investigating-all-meta-data)
* [Investigating Data Coverage](#investigating-data-coverage)
* [Investigating Stored Locations](#investigating-stored-locations)

## Investigating Stored Parameters

Following command will list stored parameters in the data:

    qdinfo -P -q /smartmet/data/ecmwf/europe/surface/querydata

Note that -q option does not require the actual querydata filename. If it is omitted, the newest querydata file in the directory is used.

The output is something like:

    The parameters stored in the querydata are:
 
    Number  Name                    Description
    ======  ====                    ===========
    1   Pressure                P
    4   Temperature             T
    5   MaximumTemperature          TMax
    6   MinimumTemperature          TMin
    10  DewPoint                Td
    13  Humidity                RH
    48  PrecipitationConv           PrecipitationConv
    51  SnowDepth               Snow Depth
    55  PrecipitationLarge          PrecipitationLarge
    59  CAPE                    CAPE
    176 SigWavePeriodBandC          Surface solar radiation [J m**-2]
    257 Evaporation             Evaporation
    264 SnowfallRate                SnowfallRate
    270 FreezingLevel               Zero degree level
    285 SoilTemperature             SoilTemperature
    472 PressureAtStationLevel          SP
    532 IceCover                IceCover
    21  -WindSpeedMS                Wind speed
    20  -WindDirection              Wind dir
    22  -WindVectorMS               Wind vector
    467 -HourlyMaximumGust          MaxGustMS
    23  -WindUMS                U
    24  -WindVMS                V
    79  -TotalCloudCover            Total Cloud Cover
    273 -LowCloudCover              Low Cloud Cover
    274 -MediumCloudCover           Medium Cloud Cover
    275 -HighCloudCover             High Cloud Cover
    271 -MiddleAndLowCloudCover         Middle+Low Cloud Cover
    353 -Precipitation1h            Precipitation mm/h
    57  -PrecipitationForm          Precipitation Form
    56  -PrecipitationType          Precipitation Type
    260 -ProbabilityThunderstorm        Probability of Thunder
    327 -FogIntensity               Density of Fog
    338 -WeatherSymbol3             Weather Symbol
    336 -WeatherSymbol1             Precipitation Symbol
 
    There are 35 stored parameters in total

## Investigating Time Information

To see time information of the data, following command can be used:

    qdinfo -t -q /smartmet/data/ecmwf/europe/surface/querydata/

The output is something like:

    Time information on the querydata:
 
    Origin time = 2011102603 local time
    First time  = 2011102603 local time
    Last time   = 2011110502 local time
    Time step   = 180 minutes
    Timesteps   = 65

By default, qdtools use local time.

## Investigating All Meta Data

To see **all** meta data of the data, following command can be used:

    qdinfo -P -q /smartmet/data/ecmwf/europe/surface/querydata

The output is omitted here since it is quite a long list.

## Investigating Data Coverage

Command

    qdinfo -x -q /smartmet/data/ecmwf/europe/surface/querydata/

returns coverage information as a text format:

    Information on the querydata area:
 
    projection      = kNFmiStereographicArea
    top left lonlat     = -48.1437,60.3753
    top right lonlat    = 73.2,55.6
    bottom left lonlat  = -13.8,31.8
    bottom right lonlat = 38.4823,29.8252
 
    fmiarea = stereographic,10,90,60:-13.8,31.8,73.2,55.6
    wktarea = PROJCS["FMI_Polar_Stereographic",GEOGCS["FMI_Sphere",DATUM["FMI_2007",SPHEROID["FMI_Sphere",6371220,0]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]],PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_orig in",60], PARAMETER["central_meridian",10],UNIT["Metre",1.0]]
 
    top = 0
    left    = 0
    right   = 1
    bottom  = 1
 
    xnumber     = 232
    ynumber     = 172
    dx      = 25.6693 km
    dy      = 25.5534 km
 
    xywidth     = 5955.27 km
    xyheight    = 4395.19 km
    aspectratio = 1.35495
 
    central longitude   = 10
    central latitude    = 90
    true latitude       = 60

Anyway, it's often handy to see this information as a map. A small tool qdview can be used to draw simple map representing the coverage of the data:

    qdview -r l -x 800 /smartmet/data/ecmwf/europe/surface/querydata ec_coverage.png

Following image:

[[images/ec_coverage.png]]

## Investigating Stored Locations

If the data contains only points (like data /smartmet/data/nmc/aws/hourly/querydata/), command

    qdinfo -X -q /smartmet/data/nmc/aws/hourly/querydata/

Output is like:

    Information on the locations:
    26106   LU              24.1159,56.9506
    26229   Ainazi              24.3665,57.8679
    26238   Rujiena             25.3716,57.8866
    26313   Kolka               22.589,57.7466
    26314   Ventspils           21.5363,57.3959
    26318   Stende              22.55,57.1836
    26324   Mersrags            23.1137,57.3333
    26326   Skulte              24.4125,57.3007
    26335   Priekuli            25.3382,57.3156
    26339   Zoseni              25.9056,57.1351
    26346   Aluksne             27.0355,57.4396
    26348   Gulbene             26.719,57.1323
    26403   Pavilosta           21.1896,56.8883
    26406   Liepaja             21.0205,56.4753
    26416   Saldus              22.5036,56.6754
    26421   Daugavgriva         24.0212,57.06
    26424   Dobele              23.3197,56.6199
    26425   Jelgava             23.7373,56.6774
    26429   Bauska              24.1832,56.415
    26435   Skriveri            25.1281,56.6426
    26436   Zilani              25.9184,56.52
    26446   Rezekne             27.2808,56.544
    26447   Madona              26.2378,56.8484
    26503   Rucava              21.1734,56.1621
    26544   Daugavpils          26.6588,55.9349

