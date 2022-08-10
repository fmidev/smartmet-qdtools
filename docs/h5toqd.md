Converts EUMETNET OPERA radar files to querydata. Only features in known use are supported.

### Usage

    h5toqd [options] infile outfile

### Options

* **-h [ --help ]**  
    print out help message
* **-v [ --verbose ]**  
    set verbose mode on
* **-V [ --version ]**  
    display version number
* **-P [ --projection ] arg**  
    projection description. See Projection descriptions in Smartmet for more details
* **-i [ --infile ] arg**  
    input HDF5 file
* **-o [ --outfile ] arg**  
    output querydata file
* **--datasetname arg**  
    dataset name prefix (default=dataset)
* **-p [ --producer ] arg**  
    producer number,name
* **--producernumber arg**  
    producer number
* **--producername arg**  
    producer name

**Known projections**

At the moment h5toqd does not have a generic proj.4 projection definition parser. Instead, the projections in known use a hard coded into the C++ source code. This will change later on as FMI implements a generic parser.

The known proj.4 descriptions are:

*    +lat_0=56.9143 +proj=aeqd +lon_0=23.9897 +a=6371000
*    +proj=aeqd +lon_0=23.9897 +lat_0=56.9143 +ellps=sphere +a=6371000
*    +proj=stere +lat_0=90 +lon_0=20 +lat_ts=60 +a=6371288 +x_0=124657.602609 +y_0=3252402.178534 +units=m

Until a generic parser is finished, new hardcoded projections can be requested from support@weatherproof.fi.
