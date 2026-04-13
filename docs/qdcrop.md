The qdcrop command is used to extract a subset querydata from the given querydata. This is commonly used in production routines to reduce the work required to calculate the desired results.

See also: [qdscript tutorial](qdscript-tutorial.md)

## Command line

The command line syntax is

    qdcrop [options] inputquerydata outputquerydata

If the output filename is omitted or it is "-", the
querydata will be output to the terminal.

The available options are

* **-V**  
Preserve querydata version number.
* **-R**  
Read all files in the input directory.
* **-g [width]x[height]+[x]+[y]**  
The subgrid to be extracted.  
For example, using 100x200+10+20 would extract a grid  
of size 100x200 starting from grid coordinates 10,20.
* **-G lon1,lat1,lon2,lat2**  
Define the minimal subgrid to be cropped by the bottom left  
and top right longitude and latitude.
* **-d [x]x[y]**  
The subgrid stepsize. For example, using 2x2 would extract  
every second grid point.
* **-P projection**  
Interpolate the cropped data to a new projection. For example: -P latlon:x1,y1,x2,y2:20x20km
* **-p param1,param2,...**  
The parameters to be extracted as a comma separated list  
of parameter names, for example Temperature,Precipitation1h or 4,353.  
By default all parameters are extracted.
* **-r param1,param2,...**  
The parameters to be removed as a comma separated list.
* **-a param1,param2,...**  
The parameters to be added to the data with missing values, unless the parameter is already defined.
* **-A param1,param2,...**  
The parameters to be copied from the origin time to all timesteps. Useful for propagating topography etc static parameters to all time steps.
* **-n oldparam1,newid1,newname1,...**  
Rename parameters using either numbers or names. The argument list must have 3 elements for each rename task.
* **-l level1,level2,...**  
The levels to be extracted as a comma separated list of
level numbers. By default all levels are extracted.
* **-S YYYYMMDDHHMI,...**  
The time stamp(s) in UTC time to be extracted.
* **-t dt1,dt2,dt**  
The time interval to be extracted as offsets from the origin
time. For example parameters 24,48 would extract times
between +24 and +48 hours from the origin time. dt1 may
be omitted, in which case it is assumed to be zero.
If the last dt parameter is given, it indicates the
desired time step (in local times).
* **-T dt1,dt2,dt**  
Same as -t, but dt is used in UTC-time mode.
* **-z yyyymmddhhmi**  
The local reference time to be used instead of origin time.
* **-Z yyyymmddhhmi**  
The UTC reference time to be used instead of origin time.
* **-i hour**  
The hour to be extracted (local time).
* **-I hour**  
The hour to be extracted (UTC time).
* **-M minute**  
The minute to be extracted (UTC time).
* **-w wmo1,wmo2-wmo3,...**  
The stations to extract from point data as identified by
the WMO numbers of the stations or ranges of them.
* **-W wmo1,wmo2-wmo3,...**  
The stations to remove from point data as identified by
the WMO numbers of the stations or ranges of them.
* **-m limit | parameter,limit**  
Maximum allowed amount of missing values for a time step to be included.
* **-N name**  
New producer name.
* **-D number**  
New producer number.

