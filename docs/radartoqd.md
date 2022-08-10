radartoqd is a special program adapted from bufrtoqd for handling specifically radar data encoded in BUFR form.

The program uses OPERA BUFR software to decode the BUFR message and then converts it to querydata.

### Command line options

The summary as reported by the program itself is as follows.

    >radartoqd -h
    Usage: radartoqd [options] infile outfile
 
    Converts Opera BUFR radar data to querydata.
 
    Allowed options:
      -h [ --help ]           print out help message
      -V [ --version ]        display version number
      -v [ --verbose ]        set verbose mode on
      -q [ --quiet ]          disable warning messages
      --debug                 print debugging information
      --allow-overflow        allow overflow in packed intensities
      -t [ --tabdir ] arg     BUFR tables directory (default=/usr/share/bufr)
      -i [ --infile ] arg     input BUFR file
      -o [ --outfile ] arg    output querydata file
      --param arg             parameter name for output
      -P [ --projection ] arg output projection
      -p [ --producer ] arg   producer number,name
      --producernumber arg    producer number (default: 1014)
      --producername arg      producer name (default: RADAR)

#### Debug mode

The debug mode is useful for displaying the details of the BUFR message. The mode will print the BUFR keys, the value field and the description of the message component.

Verbose mode is automatically set on when --debug is used.

Sample output:

    3  1   1      78.0000000   0  1   1 WMO block number
                  891.0000000   0  1   2 WMO station number
     3  1  11    2014.0000000   0  4   1 Year
                    8.0000000   0  4   2 Month
                    4.0000000   0  4   3 Day
     3  1  12      15.0000000   0  4   4 Hour
                   15.0000000   0  4   5 Minute
     3  1  23      16.3200000   0  5   2 Latitude (coarse accuracy)
                  -61.3500000   0  6   2 Longitude (coarse accuracy)
     3  1  23      16.1800000   0  5   2 Latitude (coarse accuracy)
                  -53.8800000   0  6   2 Longitude (coarse accuracy)
     3  1  23       9.0600000   0  5   2 Latitude (coarse accuracy)
                  -54.1300000   0  6   2 Longitude (coarse accuracy)
     3  1  23       9.1500000   0  5   2 Latitude (coarse accuracy)
                  -61.3500000   0  6   2 Longitude (coarse accuracy)
     0 29   1       4.0000000            Projection type
     0  5   2      16.3200000            Latitude (coarse accuracy)
     0  5  33    2000.0000000            Pixel size on horizontal - 1
     0  6  33    2000.0000000            Pixel size on horizontal - 2
     0 30  21     400.0000000            Number of pixels per row
     0 30  22     400.0000000            Number of pixels per column
     0 30  31       0.0000000            Picture type
     0 30  32       0.0000000            Combination with other data
     0 29   2       0.0000000            Co-ordinate grid type
    ...

#### Verbose mode

The verbose mode will print a summary of the metadata in the BUFR message.

Sample output:

    Radar BUFR metadata
    -------------------
    WMO Block                             = 78
    WMO Station                           = 891
 
    Year                                  = 2014
    Month                                 = 8
    Day                                   = 4
    Hour                                  = 15
    Minute                                = 15
    Position                              = -,16.32
    Height                                = -
    Height above station                  = -
 
    Projection type                       = 4 = Azimuthal equidistant
    Semi-major axis                       = -
    Semi-minor axis                       = -
    Origin                                = -,-
    False easting                         = -
    False northing                        = -
    Standard parallel 1                   = -
    Standard parallel 2                   = -
 
    Image type                            = 0
    Quality indicator                     = -
    Co-ordinate grid type                 = 0
    NW-corner                             = -61.35,16.32
    NE-corner                             = -53.88,16.18
    SE-corner                             = -54.13,9.06
    SW-corner                             = -61.35,9.15
    Pixels per row                        = 400
    Pixels per column                     = 400
    Pixel size along X-dim                = 2000
    Pixel size along Y-dim                = 2000
    Antenna elevation angle               = -
    North South organisation              = -
    East West organisation                = -
    Heights                               = -
    Calibration method                    = -
    Clutter treatment                     = -
    Ground occultation correction         = -
    Range attenuation correction          = -
    Bright-band correction                = -
    Radome attenuation correction         = -
    Clear-air attenuation correction      = -
    Precipitation attentuation correction = -
 
    dBZ scale                             = -
    intensity scale                       =[0.01,215.29,4.8,86.42,276.9,335.28,131.23,338.52,10.4,176.04,400.13,328.05,375.68,128,0.02,15.37,302.06,0.56,0.01,302.06,0,82.24,245.97,655.04,16,0.21,655.04,0.04,5.18,3.37,650.24,327.68,3.37,650.24,0.96,82.88,52.47,573.53,163.84,52.47,573.44,20.5,15.37,158.7,1.68,0.01,158.7,0,409.92,245.95,163.52,30.08,0.19,163.52,0.12,5.18,2.99,650.24,522.24,2.99,650.24,2.24,82.88,47.03,573.57,409.6,46.71,573.44,40.98,15.37,76.78,2.32,0.01,76.78,0.01,82.24,245.93,327.36,39.04,0.17,327.36,0.2,5.18,2.73,650.25,0,2.73,650.24,3.52,82.88,42.87,573.6,491.52,42.87,573.44,61.46,15.37,15.34,2.8,0.01,15.34,0.01,409.92,245.92,81.6,46.4,0.15,655.04,0.28,5.18,2.51,650.25,112.64,2.51,650.24,4.8,82.88,39.67,573.63,163.84,39.67,573.44,81.94,15.36,619.5,3.2,0,619.5,0.02,82.24,245.9,573.12,52.48,0.14,573.12,0.36,5.18,2.33,650.25,204.8,2.33,650.24,6.08,82.88,36.79,573.65,327.68,36.79,573.44,102.42,15.36,578.54,3.52,0,578.54,0.02,409.92,245.89,573.12,57.6,0.13,573.12,0.44,5.18,2.17,650.25,286.72,2.17,650.24,7.36,82.88,34.23,573.67,327.68,34.23,573.44,122.9,15.36,537.58,3.84,0,537.58,0.03,82.24,245.88,573.12,62.72,0.12,573.12,0.52,5.18,2.01,650.25,368.64]
    Z to R conversion                     = 0
    Z to R conversion factor              = 100
    Z to R conversion exponent            = 5.1
    dBZ offset (alpha)                    = -
    dbZ increment (beta)                  = -

#### Overflows

Occasionally one may encounter BUFR messages in which the packed data contains values which exceed the high limit of the palette given in the header of the message. Such cases are normally considered to be an error, and radartoqd will abort. If the option --allow-overflow is used, the highest value in the palette will be used for the overflowing encoded values.

#### Projection changes

The projection of the data may be altered on the fly by using the -P (--projection) option. The program will internally create querydata as usual, and will then create the actual output querydata from it as qdinterpolatearea was used.
