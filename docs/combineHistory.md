The combineHistory command is used to merge two or more querydata files into one large one. The assumption is that the files are otherwise similar, but have different or at least partially different time steps. The result will contain all unique time steps found from the files. If several files contain the same time step, the data is taken from the newest file.

The most common use of combineHistory is to merge several files containing observations into a larger one so that accessing observations would be easier.

## Command line

The syntax of the command line is

    combineHistory [options] directory [ directory2 directory3 _.] > output
    combineHistory [options] -O outfile directory [ directory2 directory3 _.]

Normally only one directory argument is given, and all files in
that directory will be merged. However, in connection with
option -1 it is common to use two or more directory
arguments.

The available options are

* **-v**  
    verbose mode
* **-o**  
    require all data to have the same origin time
* **-r**  
    pick the oldest origin time instead of the latest
* **-t**  
    attempt to minimize the size of the time descriptor in the result
* **-1**  
    take only the latest file from each directory
* **-O filename**  
    write the result into the given memory mapped output file instead of standard output
* **-S timestamp**  
    reference time to use instead of the wall clock
* **-p hours**  
    maximum age of allowed timesteps into the past (default = 48h)
* **-f hours**  
    maximum age of allowed timesteps into the future (default = 8h)
* **-N name**  
    new producer name
* **-D id**  
    new producer ID

