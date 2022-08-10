qdview draws the grid in a querydata or the latlon bounding box for point data.

Usage:

    qdview [options] [querydata] [pngfile]

Options:

* **-x [width]**  
    The width of the image (default 400)
* **-y [height]**  
    The height of the image
* **-m [degrees]**  
    The default margin for map data
* **-r [char]**  
    The resolution of GSHHS data to be used (default crude)
* **-s [pixels]**  
    The width of the square dot
* **-S [pixels]**  
    The width of the square dot, but only valid points are drawn
* **-t [param]**  
    The parameter to use for checking for the validity of the grid points (default=all)

If no width or height is specified, the height is calculated from the default width. If only one of width and height are specified, the other is calculated from the data projection information so that the aspect ratio is preserved. If both width and height are given, the aspect ratio may be distorted

Available GSHHS resolutions are:

* crude resolution (25km)
* low resolution (5 km)
* intermediate resolution (1km)
* high resolution (0.2km)
* full resolution

Example:

    qdview -x 800 -r l /smartmet/data/ecmwf/europe/surface/querydata ec_coverage.png

Following image:

[[images/ec_coverage.png]]
