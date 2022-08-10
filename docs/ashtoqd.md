ashtoqd Converts MetOffice CSV ash advisories to querydata.

### Usage

    ashtoqd [options] indir outfile

### Allowed options

* **-h [ --help ]**  
    print out help message
* **-v [ --verbose ]**  
    set verbose mode on
* **-V [ --version ]**  
    display version number
* **-t [ --time ] arg**  
    model run time
* **-P [ --projection ] arg**  
    projection description. See Projection descriptions in Smartmet for more details
* **-i [ --indir ] arg**  
    input CSV directory
* **-o [ --outfile ] arg**  
    output querydata file
* **-b [ --boundary ]**  
    extract ash boundaries instead of concentrations
* **-p [ --producer ] arg**  
    producer number,name
* **--producernumber arg**  
    producer number
* **--producername arg**  
    producer name

Note that ash concentration advisories and ash boundary advisories usually have different model runs. Hence the converter does not try to put both parameters into the same querydata. It is recommended to run the converter twice for the same directory, the latter run with option -b, and direct the output to different directories.
