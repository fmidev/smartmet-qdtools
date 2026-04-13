qdcombine combines all querydata-files from given directory to one file if possible. It makes combined times/params/level to output file. Querydata in files must have the same projection and grid size unless point data is being combined.

If multiple arguments are given, each one is processed as a file, or as the latest file in a directory.

### Usage

    qdcombine [options] directory [file2 file3...] > output
    qdcombine [options] -o outfile directory [file2 file3...]
    qdcombine [options] -O mmapped_outfile directory [file2 file3...]

### Options

* **-o outfile**  
    Output filename (default: stdout).
* **-O outfile**  
    Memory mapped output filename.
* **-P**  
    Combine point data instead of grid data.
* **-l levelType[,value]**  
    Set level type and value (e.g. 5000,0 would be normal ground data).
* **-p id,name**  
    Set producer id and name (e.g. 240,ecmwf).

