Converts BUFR observations to querydata.

### Usage

    bufrtoqd [options] infile/dir outfile

### Options

* **-h [ --help ]**  
    print out help message
* **-V [ --version ]**  
    display version number
* **-v [ --verbose ]**  
    set verbose mode on
* **--debug**  
    set debug mode on
* **--subsets**  
    decode all subsets, not just first ones
* **-c [ --config ] arg**  
    BUFR parameter configuration file (default=/smartmet/cnf/bufr.conf)
* **-s [ --stations ] arg**  
    stations CSV file (default=/smartmet/share/csv/stations.csv)
* **-i [ --infile ] arg**  
    input BUFR file or directory
* **-o [ --outfile ] arg**  
    output querydata file
* **-m [ --message ] arg**  
    extract the given message number only
* **-B [ --localtableB ] arg**  
    local BUFR table B
* **-D [ --localtableD ] arg**  
    local BUFR table D
* **-p [ --producer ] arg**  
    producer number,name
* **--producernumber arg**  
    producer number
* **--producername arg**  
    producer name

