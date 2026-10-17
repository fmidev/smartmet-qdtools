qdcheck is used to check missing values and minmax-limits to parameters. Also data times can be checked. Controlfile defines warning- and error-limits.

Return codes are:

   * 0=OK
   * 3=Warning
   * 2=error
   * 1=fatalerror.

Usage:

    qdcheck [options] controlfile datafile resultfile

Options:

* **-h [ --help ]**  
    print out help message
* **-V [ --version ]**  
    display version number
* **--check-dewpoint-difference**  
    enable the temperature >= dew point check
* **--config arg**  
    configuration file (same as the controlfile argument)
* **--infile arg**  
    input querydata (same as the datafile argument)
* **--outfile arg**  
    output report filename (same as the resultfile argument)