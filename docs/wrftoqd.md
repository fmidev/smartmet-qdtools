The nctoqd program can handle regular NetCDF data without any model specific extensions

wrftoqd converts the WRF NetCDF data to querydata using a special version of the nctoqd sources.

### Command line

The summary of command line options as given by the program is

    >wrftoqd -h
    Usage: wrfoqd [options] infile outfile
 
    Converts Weather Research and Forecasting (WRF) Model NetCDF to querydata.
 
    Allowed options:
      -h [ --help ]                       print out help message
      -v [ --verbose ]                    set verbose mode on
      -V [ --version ]                    display version number
      -i [ --infile ] arg                 input netcdf file
      -o [ --outfile ] arg                output querydata file
      -c [ --config ] arg                 NetCDF CF standard name conversion table
                                          (default='/usr/share/smartmet/formats/net
                                          cdf.conf')
      -t [ --timeshift ] arg              additional time shift in minutes
      -p [ --producer ] arg               producer number,name
      --producernumber arg                producer number
      --producername arg                  producer name
      -s [ --fixstaggered ]               modifies staggered data to base form
      -u [ --ignoreunitchangeparams ] arg ignore unit change params
      -x [ --excludeparams ] arg          exclude params
      -P [ --projection ] arg             final data area projection
