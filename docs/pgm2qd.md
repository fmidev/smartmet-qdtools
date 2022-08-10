pgm2qd converts pgm files into querydata. The files are expected to contain metadata in the header comments do determine the projection and other details of the data

### Usage

    pgm2qd [options] inputdata outputdata

### Options

* **-h**  
    print this usage information
* **-v**  
    verbose mode
* **-f**  
    force rewrite if already converted
* **-t dir**  
    temporary work directory to use (default is /tmp)
* **-p int,name**  
    set producer id and name (default = 1014,NRD)
* **-l minutes**  
    set age limit for input files (default = none)

### Metadata

PGM files are allowed to have a comment section in the header. This is utilized by pgm2qd for enabling the conversion of PGM grayscale data into querydata. This has historically been used for example to convert FMI radar data into querydata.

A sample header follows:

    >head -12 /data/fmi/raakadata/tutka/skandinavia/dbz/201112151845_tutka_skandinavia_dbz.pgm
    P5
    # obstime 201112151845
    # param CorrectedReflectivity
    # projection radar {
    # type stereographic
    # centrallongitude 25
    # centrallatitude 90
    # truelatitude 60
    # bottomleft 6.485 51.113
    # topright 37.162 72.002
    # }
    874 1081

One can note that the image begins with the expected P5 signature, and is followed by several lines of comments, as identified by the '#'-character. Once the comment section ends, the dimensions of the image are given. Raw PGM data would begin at the next row.

The keywords recognized by pgmtoqd in the comment section are:

* obstime timestamp: the valid time of the data in UTC time
* fortime timestamp: the forecast time of the data in UTC time
* projection: a block containing a projection definition is expected next, as explained later on.
* param name: the Smartmet parameter name for the data
* multiplier value: multiplier for the greyscale value to get actual value, the 'a' in in a*x+b
* offset value: offset for the greyscale value to get actual value, the 'b' in a*x+b
* level type,value: level type and value
  * type h = height  
  * type hpa = pressure level

The projection is described in a block surrounded by curly braces {}. The known keywords in the block are

*    type name
*    centrallatitude lon
*    centrallongitude lat
*    truelatitude lat
*    bottomleft lon lat
*    topright lon lat
*    center lon lat
*    scale scale

The values are in exact correspondence with the [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md). In fact, pgm2qd was implemented before other programs adopted the more compact form.
