qdinterpolatetime interpolates data to desired time resolution (also to lower time resolution despite of name).

Usage:

    qdinterpolatetime [options] timeResInMinutes [startTimeResInMinutes=32700] [maxSearchRangeInMinutes=360 (min)
    [generalInterpolationMethod=1 (=linearly, 5=lagrange)]

Options:

* **-i infile**  
    Read the given input file instead of standard input
* **-o outfile**  
    Write to the given file instead of standard output

The default value for both the input and the output file is '-', indicating standard input and standard output.

For example:

    qdinterpolatetime 60 < input.sqd > output.sqd

Modifies data to one hour time resolution. All parameters, places and levels are processed and reserved.
