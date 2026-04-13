Converts the output of FMI's Kriging analysis programs (ASCII format) to querydata.

### Usage

    kriging2qd [options] inputfile outputfile

If the input argument is a directory, the newest file in it is converted.

### Input format

The input file is expected to contain data in YKJ-coordinates in X Y VALUE order like this:

    3405000 6705000     3.0     3.5    ...
    3415000 6705000     7.6     7.6
    3425000 6705000     6.4     6.4
    ...

The ordering of the coordinates does not matter, the program will automatically establish the limits and size of the grid. The program will abort if the grid cannot be determined automatically. Any row starting with character '#' will be discarded.

The number of columns after the 2 coordinate columns must match the number of parameters given using option -p.

### Options

* **-h**
    Print help information
* **-v**
    Verbose mode
* **-p paramname1,paramname2,...**
    Specify the parameter names (default: Temperature)
* **-t stamp**
    Specify the data time in UTC (default: now)
* **-T stamp**
    Specify origin time (overrides -t)
