The qdproject command is used for transforming geographic coordinate information between three different coordinate systems:

1. Regular latitude-longitude coordinates
2. World coordinates (projection specific metric distances)
3. Pixel coordinates (world coordinates scaled down to an image)
The command is useful for example when

* You need to render something onto an image at a specific latlon coordinate
* You need to know at which latlon the user clicked on some map

See also [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md)

## Command line

The command line syntax is

    qdproject [options] x1,y1 x2,y2 x3,y3 ...

One may give as many x,y pairs on the command line as wishes, but in practise the number is limited by the maximum allowed size of the command line in the shell.

If one needs to convert a large number of coordinates, it is more convenient to place the coordinate pairs into a file. The file can then be fed to qdproject as follows:

    qdproject [options] < filename

qdproject will print the coordinate pairs on separate rows, with the x-y values separated by a space.

The available options are:

* **-h**  
    print a brief summary of the available options
* **-v**  
    verbose mode - the original input is printed along with the projected values
* **-d proj**  
    a string describing the projection. The format is documented in qdcontour manual.
* **-c filename**  
    a file containing the projection description
* **-q filename**  
    reference querydata in the desired projection
* **-l**  
    convert lon-lat pairs to pixel coordinates
* **-L**  
    convert lon-lat pairs to world coordinates
* **-i**  
    convert pixel coordinates to lon-lat coordinates
* **-I**  
    convert world coordinates to lon-lat coordinates

For example, for Scandinavia FMI often uses a projection description of the form

    stereographic,20:6,51.3,49,70.2:600,-1

which describes a polar stereographic projection with central longitude 20°. The bottom left coordinate is at 6°E 51.3°N, the top right at 49°W 70.2°N. The width of the image is 600 pixels, the height will be calculated automatically so that the aspect ratio of the map is correct.

A sample input for the -c option would be file

    /smartmet/share/maps/default/latvia_600x288/area.conf

whose contents is

    projection latlon:20,55,29,59:600,-1

## Examples

If a projection description is:

    projection latlon:20,55,29,59:600,-1

The coordinates of Riga are 56.946°N 24.1059°E. The pixel coordinates of Riga in the image is then obtained with

    > qdproject -l -d latlon:20,55,29,59:600,-1 - 59.946,24.1059
     273.7266667 137.1045

Note that the pixel coordinate is measured from the upper left corner. This is common in graphics, using bottom left corner is more common in mathematics and physics.

Note also the use of the special "-" marker to indicate the end of the command line. Without it qdproject would try to interpret the string 56.946,24.1059 as a command line option due to the leading minus character.

The inverse result is obtained like this:

    > qdproject -i -d latlon:20,55,29,59:600,-1 -  273.7266667 137.1045
     56.946 24.1059
