qdtogrib is used to convert querydata to GRIB-format. Both GRIB1 and GRIB2 are supported.

Usage:

    qdtogrib [options] infile outfile

Options:

|Short option   |Long option   |Arguments   |Description   |
|---------------|--------------|------------|--------------|
|-h|--help||print out help message|
|-V|--version||display version number|
|-v|--verbose||set verbose mode on|
|-D|--dump||dump GRIB contents using the grib_api dumper|
|-i|--infile|filename|input querydata|
|-o|--outfile|filename|output grib file|
|-1|--grib1||output GRIB1|
|-2|--grib2||output GRIB2 (the default)|
|-p|--packing|method|packing method (grid_simple, grid_ieee, grid_second_order, grid_jpeg etc)|
|-C|--centre|centre|originating centre (default = none)|
|-S|--subcentre|subcentre|subcentre (default = 0)|
|-L|--list-centres||list known centres|
|-d|--delete||ignore parameters which are not listed in the config|
|-I|--ignore-origintime||use first valid time instead of origin time as the forecast time|
|-s|--split||split individual timesteps|
|-l|--level|value|level to extract|
|-c|--config|filename|configuration file with conversion information (default=/usr/share/smartmet/formats/grib.conf)|

Note that the option parser used by many of the Smartmet tools allows one to specify expected command line arguments also via options, if there are no ambiguities. Hence for example one may omit outfile argument, and use -o outfile instead.