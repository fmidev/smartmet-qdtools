The qddifference program reads in two querydata files and prints out how much they differ.

### Usage

    qddifference [options] querydata1 querydata2

If the input argument is a directory, the newest file in it is used.

The program assumes the two files have the same levels, locations, times and parameters. If not, the program will not crash, but the analysis will be pretty meaningless.

### Options

* **-h**
    Print help information
* **-t**
    Analyze each timestep separately
* **-p**
    Calculate percentage of different grid points instead
* **-P param1,param2,...**
    Specify the parameters to check
* **-e epsilon**
    Maximum allowed difference
