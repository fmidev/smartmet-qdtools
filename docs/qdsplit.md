qdsplit is used in production scripts to split querydata into separate timesteps. For example, when producing animations we may then render images for several timesteps in parallel.

## Command line

The command line syntax is

    qdsplit [options] querydata outputdir

Options are:

* **-h**  
    prints a brief summary of the command line syntax
* **-v**  
    verbose mode, the program prints out what it is doing
* **-s**  
    short filename mode, only the timestamp is used The querydata argument can be either a filename or a directory, in which case the newest file in the directory is used. The original name of the file is used to construct the names of the output filenames like this YYYYMMDDHHMI_originalname unless option -s is used, in which case the filenames will be of the format YYYYMMDDHHMI.sqd

## Examples

Here is sample output when splitting GFS forecasts:

    > qdsplit -v /smartmet/data/gfs/europe/surface/querydata test/
    Reading ' /smartmet/data/gfs/europe/surface/querydata/200610100514_gfs_world_surface.sqd'
    Writing 'test//200610100300_200610100514_gfs_world_surface.sqd
    Writing 'test//200610100600_200610100514_gfs_world_surface.sqd
    Writing 'test//200610100900_200610100514_gfs_world_surface.sqd
    Writing 'test//200610101200_200610100514_gfs_world_surface.sqd
    ...

Here is the output when using the -s option:

    > qdsplit -v -s /smartmet/data/gfs/europe/surface/querydata test/
    Reading '/smartmet/data/gfs/europe/surface/querydata/200610100514_gfs_world_surface.sqd'
    Writing 'test//200610100300.sqd
    Writing 'test//200610100600.sqd
    Writing 'test//200610100900.sqd
    Writing 'test//200610101200.sqd
    ...
