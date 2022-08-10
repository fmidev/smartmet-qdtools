The qdcrop command is used to extract a subset querydata from the given querydata. This is commonly used in production routines to reduce the work required to calculate the desired results.

See also: [qdscript tutorial](qdscript-tutorial.md)

## Command line

The command line syntax is

    qdcrop [options] inputquerydata outputquerydata

If the output filename is omitted or it is "-", the
querydata will be output to the terminal.

The available options are

* **-g [width]x[height][x][y]**  
The subgrid to be extracted.  
For example, using 100x200+10+20 would extract a grid  
of size 100x200 starting from grid coordinates 10,20.
* **-G lon1,lat1,lon2,lat2**  
Define the minimal subgrid to be cropped by the bottom left  
and top right longitude and latitude.
* **-d [x]x[y]**  
The subgrid stepsize. For example, using 2x2 would extract  
every second grid point.
* ***-P projection**  
Interpolate the cropped data to a new projection. For example: -P latlon:x1,y1,x2,y2:20x20km
* **-p param1,param2,...**  
The parameters to be extracted as a comma separated list  
of parameter names, for example Temperature,Precipitation1h or 4,353.  
By default all parameters are extracted.
* **-r param1,param2,...**  
The parameters to be removed as a comma separated list
* **-a param1,param2,...**  
The parameters to be added to the data with missing values.
* **-A param1,param2,...**  
The parameters to kept with the values taken from the first time step (presumably analysis time). Useful for propagating topography etc static parameters to all time steps.
* **-l level1,level2,...**  
The levels to be extracted as a comma separated list of
level numbers. By default all levels are extracted.
* **-S yymmddhhmi,....**  
The time stamp(s) in UTC time to be extracted.
* **-t dt1,dt2,dt**  
The time interval to be extracted as offsets from the origin
time. For example parameters 24,48 would extract times
between +24 and +48 hours from the origin time. dt1 may
be omitted, in which case it is assumed to be zero.
if the last dt parameter is given, it indicates the
desired time step (in local times)
* **-T dt1,dt2,dt**  
Same as -t, but dt is used in UTC-time mode
* **-z yyyymmddhhmi**  
The local reference time to be usead instead of origin time
* **-Z yyyymmddhhmi**  
The UTC reference time to be usead instead of origin time
* **-i hour**  
The hour to be extracted (local time)
* **-I hour**
The hour to be extracted (UTC time)
* **-w wmo1,wmo2,...**
The stations to extract from point data as identified by
the WMO numbers of the stations
* **-m limit**  
Maximum allowed percentage of missing values per timestep in output.

