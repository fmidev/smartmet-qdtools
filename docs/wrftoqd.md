The nctoqd program can handle regular NetCDF data without any model specific extensions

wrftoqd converts the WRF NetCDF data to querydata using a special version of the nctoqd sources.

### Command line

The summary of command line options as given by the program is

    >wrftoqd -h
    Usage: wrfoqd [options] infile outfile
 
    Converts Weather Research and Forecasting (WRF) Model NetCDF to querydata.
 
    Allowed options:
      -h [ --help ]                       print out help message
      -c [ --config ] arg                 Replacement NetCDF CF standard name
                                          conversion table (default='/usr/share/smar
                                          tmet/formats/netcdf.conf')
      -C [ --configs ] arg                Extra NetCDF name conversions (take
                                          precedence over standard names)
      -n [ --conventions ] arg            Minimum NetCDF conventions to verify or
                                          empty string if no check wanted (default:
                                          CF-1.0)
      -d [ --debug ]                      enable debugging output
      --tdim arg                          name of T-dimension parameter
                                          (default=time)
      --xdim arg                          name of X-dimension parameter
                                          (default=west_east)
      --ydim arg                          name of Y-dimension parameter
                                          (default=south_north)
      --zdim arg                          name of Z-dimension parameter (default=)
      --info                              print information on data dimensions and
                                          exit
      --experimental                      enable experimental features
      -i [ --infile ] arg                 input netcdf file
      -a [ --globalAttributes ] arg       netCdf data's cmd-line given global
                                          attributes
      --mmap                              memory map output file to save RAM
      -o [ --outfile ] arg                output querydata file
      -m [ --parameter ] arg              define parameter conversion(same format as
                                          in config)
      -p [ --producer ] arg               producer number,name
      -P [ --projection ] arg             final data area projection
      --producernumber arg                producer number
      --producername arg                  producer name
      -s [ --fixstaggered ]               modifies staggered data to base form
      -u [ --ignoreunitchangeparams ] arg ignore unit change params
      -t [ --timeshift ] arg              additional time shift in minutes
      -T [ --tolerance ] arg              axis stepping tolerance
      -U [ --autoids ]                    generate ids automatically for unknown
                                          parameters starting from id 30000
      -v [ --verbose ]                    set verbose mode on
      -V [ --version ]                    display version number
      -x [ --excludeparams ] arg          exclude params
      --addparams arg                     add parameters by calculating them
