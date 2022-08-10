qdtogrib is used to convert querydata to GRIB-format. Both GRIB1 and GRIB2 are supported.

Usage:

    qdtogrib [options] infile outfile

Options:

|Short option   |Long option   |Arguments   |Description   |
|---------------|--------------|------------|--------------|
|-h|--help||print out help message|
|-V|--version||display version number|
|-v|--verbose||set verbose mode on|
|-i|--infile|filename|input querydata|
|-o|--outfile|filename|output grib file|
|-1|--grib1||output GRIB1|
|-2|--grib2||output GRIB2 (the default)|
|-p|--params|old1,new1,old2,new2...|parameter conversion list|
|-s|--split||output each timestep into a separate file|
|-l|--level|value|level to extract|

Note that the option parser used by many of the Smartmet tools allows one to specify expected command line arguments also via options, if there are no ambiguities. Hence for example one may omit outfile argument, and use -o outfile instead.