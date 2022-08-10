qdcombine combines all querydata-files from given directory to one file if possible. It makes combined times/params/level to output file. Querydata in files must have the same projection and grid size.

Usage:

    qdcombine [options] directory > output

Options:

* **-l levelType,value**  
    force leveltype to specified one (e.g. 5000,0 would be normal ground data).
* **-p id,name**  
    Make result data's producer id and name as wanted. (e.g. 240,ecmwf)

