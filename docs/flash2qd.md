flash2qd is used to convert ascii flash files to querydata.

### Usage

    flash2qd [-s lineCount] [-t] flashData > flash.sqd

### Options

* **-s** **<****lineCount****>**  
    Lines to skip from start of file, default = 0
* **-t**
    Make time conversion from local to utc, default = no conversion

### Example usage

    flash2qd -s 1 myflashdata.txt > flash.sqd

### Input data syntax

Given input data have to be in following form (separated with tabs (\t)) :

    YYYYMMDDHHmmss lon lat power multiplicity accuracy

For example:

    20040505144234  19.984  53.199  -28 1   43.5
    20040505144925  19.457  53.563  -30 1   29.5
    20040505150731  19.246  53.853  23  1   39.2
    20040505151128  29.938  63.529  -3  1   10.4

