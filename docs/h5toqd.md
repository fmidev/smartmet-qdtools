Converts EUMETNET OPERA radar files to querydata. Only features in known use are supported.

### Usage

    h5toqd [options] infile outfile

### Options

* **-h [ --help ]**  
    print out help message
* **-v [ --verbose ]**  
    set verbose mode on
* **--quietproj**  
    disable PROJ error messages
* **-V [ --version ]**  
    display version number
* **-P [ --projection ] arg**  
    projection description. See Projection descriptions in Smartmet for more details
* **-i [ --infile ] arg**  
    input HDF5 file
* **-o [ --outfile ] arg**  
    output querydata file
* **--prodparfix**  
    take the last number from the prodpar array
* **--lowercase**  
    convert the output filename to lower case
* **--uppercase**  
    convert the output filename to upper case
* **--datasetname arg**  
    dataset name prefix (default=dataset)
* **-p [ --producer ] arg**  
    producer number,name
* **--producernumber arg**  
    producer number (default: 1014)
* **--producername arg**  
    producer name (default: RADAR)
* **--startepochs**  
    store the HDF5 startepochs as the valid time
* **--default-plc arg**  
    default replacement for %PLC (default=comp)
* **--default-wmo arg**  
    default replacement for %WMO (default=wmo)
* **--default-rad arg**  
    default replacement for %RAD (default=rad)
* **--default-nod arg**  
    default replacement for %NOD (default=nod)
* **--default-org arg**  
    default replacement for %ORG (default=org)
* **--default-cty arg**  
    default replacement for %CTY (default=cty)

**Output file name**

The output filename may contain the following strings, which will be replaced by the respective settings according to the ODIM specifications:

*    %ORIGINTIME
*    %PRODUCT - taken from product
*    %QUANTITY - taken from quantity
*    %WMO, %RAD, %PLC, %NOD, %ORG, %CTY - taken from the respective source components
*    %INTERVAL - enddate,endtime - startdate,starttime or empty if not available or zero

If the ODIM setting is missing, a default replacement will be used. The defaults can be changed using the --default-* options.

**Projections**

For composites, images and Cartesian volumes (object COMP, IMAGE or CVOL) the projection is read from the /where/projdef attribute, and the area from the corner coordinates (LL/UR or UL/LR) in /where. Polar volumes (PVOL) are converted to an azimuthal equidistant grid centred on the radar, with the range and resolution determined from the data. Option -P can be used to project the result to a different area.
