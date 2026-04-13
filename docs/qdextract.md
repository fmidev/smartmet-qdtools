Extract point data from gridded data or point data.

### Usage

    qdextract [options] locations [inputdata [outputdata]]

I/O can be specified in multiple ways:

    qdextract -i input.sqd -o output.sqd locations
    qdextract locations input.sqd output.sqd
    qdextract locations < input.sqd > output.sqd

Using '-' as input or output filename implies stdin/stdout.
Using a directory name or a filename enables memory mapping for efficiency.

### Options

* **-h [ --help ]**
    Print out help message
* **-V [ --version ]**
    Display version number
* **-i [ --infile ] arg**
    Input querydata
* **-o [ --outfile ] arg**
    Output querydata
* **-l [ --locations ] arg**
    Location descriptions file

### Location file format

Location files may contain C++ style comments. Once stripped, the file
is expected to begin with a number describing the format of the locations
as follows:

* **1:** a NFmiLocationBag C++ object follows
* **2:** a location list follows with rows of form: id, unused line, name, lon lat
* **3:** a free form location list with the following order of fields: id name lon lat

Using format 3 is recommended for clarity.
