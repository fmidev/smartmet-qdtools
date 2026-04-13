Converts CF-1.4 conforming NetCDF to querydata. Only features in known use are supported.

### Usage

    nctoqd [options] infile outfile

### Options

* **-h [ --help ]**  
    print out help message
* **-v [ --verbose ]**  
    set verbose mode on
* **-V [ --version ]**  
    display version number
* **-i [ --infile ] arg**  
    input netcdf file
* **-o [ --outfile ] arg**  
    output querydata file
* **-c [ --config ] arg**  
    configuration file for parameter name conversions
* **-C [ --configs ] arg**  
    extra NetCDF name conversions (take precedence over standard names)
* **-n [ --conventions ] arg**  
    minimum NetCDF conventions to verify, or empty string if no check wanted (default: CF-1.0)
* **-d [ --debug ]**  
    enable debugging output
* **-t [ --timeshift ] arg**  
    additional time shift in minutes
* **-p [ --producer ] arg**  
    producer number,name
* **-P [ --projection ] arg**  
    final data area projection
* **--producernumber arg**  
    producer number
* **--producername arg**  
    producer name
* **-m [ --parameter ] arg**  
    define parameter conversion (same format as in config)
* **-a [ --globalAttributes ] arg**  
    netCDF data's command-line given global attributes
* **-s [ --fixstaggered ]**  
    modifies staggered data to base form
* **-u [ --ignoreunitchangeparams ] arg**  
    ignore unit change params
* **-T [ --tolerance ] arg**  
    axis stepping tolerance
* **-U [ --autoids ]**  
    generate ids automatically for unknown parameters
* **-x [ --excludeparams ] arg**  
    exclude params
* **--addparams arg**  
    add parameters by calculating them
* **--tdim arg**  
    name of T-dimension parameter
* **--xdim arg**  
    name of X-dimension parameter
* **--ydim arg**  
    name of Y-dimension parameter
* **--zdim arg**  
    name of Z-dimension parameter
* **--info**  
    print information on data dimensions and exit
* **--experimental**  
    enable experimental features
* **--mmap**  
    memory map output file to save RAM

The default configuration file for parameter name conversions is

    /smartmet/cnf/netcdf.conf

If the parameter name is not found from the conversion table, nctoqd will try to match the name to known newbase names. If there is no success, the parameter will be skipped, and the skipping will be reported to standard output if verbose mode on.

A sample configuration file:

    #  http://cf-pcmdi.llnl.gov/documents/cf-standard-names/ecmwf-grib-mapping
    # a,b means translation of NetCDF a to newbase b
    # x,y,f,d means translation of NetCDF x and y components to speed f and direction d
    air_pressure,PressureAtStationLevel
    air_pressure_at_sea_level,Pressure
    air_temperature_anomaly,TemperatureAnomaly
    ait_temperature,Temperature
    dew_point_temperature,DewPoint
    direction_of_sea_ice_velocity,IceDirection
    direction_of_sea_water_velocity,CurrentDirection
    eastward_sea_water_velocity,northward_sea_water_velocity,CurrentSpeed,CurrentDirection
    eastward_wind,northward_wind,WindSpeedMS,WindDirection
    geopotential_height,GeopHeight
    geopotential_height_anomaly,GeopotentialHeightAnomaly
    high_cloud_area_fraction,HighCloudCover
    low_cloud_area_fraction,LowCloudCover
    medium_cloud_area_fraction,MediumCloudCover
    precipitation_flux,PrecipitationRate
    relative_humidity,Humidity
    sea_ice_area_fraction,IceCover
    sea_ice_eastward_velocity,sea_ice_northward_velocity,IceSpeed,IceDirection
    sea_ice_speed,IceSpeed
    sea_ice_thickness,IceThickness
    sea_ice_x_velocity,sea_ice_y_velocity,IceSpeed,IceDirection
    sea_surface_height_above_geoid,SeaLevel
    sea_water_potential_temperature,TemperatureSea
    sea_water_salinity,Salinity
    sea_water_speed,CurrentSpeed
    sea_water_temperature,TemperatureSea
    sea_water_x_velocity,sea_water_y_velocity,CurrentSpeed,CurrentDirection
    soil_temperature,SoilTemperature
    specific_humidity,SpecificHumidity
    surface_snow_thickness,SnowDepth
    visibility_in_air,Visibility
    water_vapour_pressure,VapourPressure
    wind_from_direction,WindDirection
    wind_speed,WindSpeedMS

