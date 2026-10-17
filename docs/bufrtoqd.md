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
    deprecated and ignored: all subsets are always decoded
* **--insignificant**  
    extract also insignificant sounding levels
* **--roundtohours arg**  
    round times to multiples of the given number of hours
* **-c [ --config ] arg**  
    BUFR parameter configuration file (default=/usr/share/smartmet/formats/bufr.conf)
* **-s [ --stations ] arg**  
    stations CSV file (default=/usr/share/smartmet/stations.csv)
* **-i [ --infile ] arg**  
    input BUFR file or directory
* **-o [ --outfile ] arg**  
    output querydata file
* **-m [ --message ] arg**  
    extract the given message number only
* **-C [ --category ] arg**  
    extract the given category
* **-B [ --localtableB ] arg**  
    local BUFR table B
* **-D [ --localtableD ] arg**  
    local BUFR table D
* **-p [ --producer ] arg**  
    producer number,name
* **--producernumber arg**  
    producer number (default: 0)
* **--producername arg**  
    producer name (default: UNKNOWN)
* **-a [ --autoproducer ]**  
    guess producer automatically
* **--usebufrname**  
    use BUFR parameter name instead of parameter code number from configuration file
* **-I [ --Ident ]**  
    load only amdar messages having aircraft reg.nr or other identification
* **-r [ --remap ] arg**  
    remap amdar bufr message codes to first code; name1,code1,code2,code3;name2,code1,...
* **-N [ --minobservatios ] arg**  
    minimum number of observations for acceptable amdar (default=3)
* **-M [ --maxdurationhours ] arg**  
    maximum amdar takeoff/landing phase duration in hours (default=2), exceeding data ignored
* **-t [ --totalcloudoctas ]**  
    disable octas to percentage conversion for TotalCloudCover
* **-f [ --forcepressurechangesign ]**  
    set PressureChange sign to match PressureTendency value

If option --insignificant is used, sounding levels with a zero extended vertical significance (code 8042) value will be extracted. This may increase the size of high resolution sounding from a hundred levels to thousands, and thus significantly increase the size of the output querydata.

The known data category names for option -C are 'land' or 'land surface', 'sea' or 'sea surface', 'sounding', 'satellite sounding', 'upper air level', 'upper air level with satellite', 'radar', 'synoptic', 'physical', 'dispersal', 'radiological', 'tables', 'satellite surface', 'radiances', 'oceanographic' and 'image'. Conversion of all categories is not supported though.

