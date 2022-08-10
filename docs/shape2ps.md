shape2ps command is used for rendering PostScript maps out of ESRI shapedata. It can also be used for rendering contours out of querydata.

Table of Contents:

* [Command line](#command-line) 
* [Control file format](#control-file-format)
* [Special keywords](#special-keywords)
  * [projection](#projection)
  * [body](#body)
  * [boundingbox](#boundingbox)
  * [shape](#shape)
  * [subshape](#subshape)
  * [gshhs](#gshhs)
  * [graticule](#graticule)
  * [exec](#exec)
  * [qdexec](#qdexec)
  * [project](#project)
  * [location](#location)
  * [system](#system)
  * [querydata](#querydata)
  * [parameter](#parameter)
  * [level](#level)
  * [timemode](#timemode)
  * [time](#time)
  * [smoother](#smoother)
  * [contourcommands](#contourcommands)
  * [contourline](#contourline)
  * [contourfill](#contourfill)
  * [bezier](#bezier)

See also [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md)

## Command line

The command line syntax is

    shape2ps controlfile

The result will be printed to standard output, so it is more common to use the command line this:

    shape2ps controlfile > result.ps

Often one converts the PostScript into a PNG-file with

    convert result.ps result.png

## Control file format

The control file is basically written in the PostScript language ([manual](https://www.adobe.com/products/postscript/pdfs/PLRM.pdf)). The only exceptions are lines beginning with special control words, and that shape2ps will generate the necessary header information based on the settings specified by those special control words.

The basic structure of the control file is always this:

    projection [some description]
    [other header settings]
    body
    [rendering stuff]

Here is a short sample control file used for rendering the Latvian map.

    projection stereographic,25:20.53,55.5,28.7,58.5:600,-1
 
    % common settings into the header
 
    /m{moveto}def
    /l{lineto}def
    /cp{closepath}def
    /rot{3 1 roll}def
    /rgb{255 div rot 255 div rot 255 div rot setrgbcolor}def
     
    /water{0 75 140 rgb}def
    /land{120 130 33 rgb}def
    /countryborder{0 0 0 rgb}def
    /shoreline{0 0 0 rgb}def
    /road{120 80 0 rgb}def
    /city{120 80 0 rgb}def
    /roadalpha{0.5 setgray}def
     
    /background{-10000 -10000 m 10000 -10000 l 10000 10000 l -10000 10000 l closepath}def
    /sea{water background fill}def
     
    body
     
    % common settings into the beginning of the body
     
    clipmargin 5
     
    0 setlinejoin
    1 setlinecap
    2 setmiterlimit
    1 setlinewidth
 
    sea
 
    land
    shape m l cp /smartmet/share/gis/shapes/ESRI/europe/country_shorelines
    gsave eofill grestore
    0 setgray 0 setlinewidth stroke
 
    water
    shape m l cp /smartmet/share/gis/shapes/ESRI/europe/mjwater
    shape m l cp /smartmet/share/gis/shapes/ESRI/europe/water
    eofill
 
    gsave
     shape m l cp /smartmet/share/gis/shapes/ESRI/estonia/country
     background clip newpath
     road
     0.5 setlinewidth
     shape m l cp /smartmet/share/gis/shapes/ESRI/europe/mjroads_roads
     stroke
     0 setlinewidth
     shape m l cp /smartmet/share/gis/shapes/ESRI/europe/roads_roads
     stroke
    grestore
 
    gsave
     shoreline
     1.5 setlinewidth
     shape m l cp /smartmet/share/gis/shapes/ESRI/europe/country_borders
     stroke
    grestore

The result looks like this:

[[images/background.png]]

## Special keywords

### projection

The command syntax is:

    projection [description]

The syntax of the projection description is documented in qdcontour manual.
The projection must be specified before the body tag is used.

### body

The command syntax is:

    body

The command is used to mark where the PostScript header stops and where the body starts.

### boundingbox

The command adds to the current path a rectangle the size of the bounding box. The command can be used for example to

*    fill the background with a color
*    stroke the bounding box to aid working with Adobe Illustrator, which normally does not limit rendering to the bounding box like most programs
 
### shape

The command syntax is:

    shape [moveto] [lineto] [closepath] [shape]

The commands produces a path description from the given shape using the given strings for the places where a moveto, a lineto or a closepath PostScript command would be suitable. It is common practise to define the aliases:

    /m{moveto}def
    /l{lineto}def
    /cp{closepath}def

in the header of the control file, and then call shape with

    shape m l cp [shape]

The idea is to reduce the size of the resulting PostScript file by defining shorthand aliases for the PostScript commands.

### subshape

The command syntax is

    subshape moveto lineto closepath attributecondition shape

The command works just like the shape command except that one can provide a condition for the shape elements to be included. Examples:

    COUNTRY=Finland
    LEVEL<=3

### gshhs

The gshhs command is very similar to the shape command. The only difference is that the data source is not an ESRI shape file, but a file in the global shoreline database format (GSHHS).

The command syntax is

    gshhs [moveto] [lineto] [closepath] [gshhsfile]

### graticule

The graticule command draws a georeference grid.
The command syntax is

    graticule [moveto] [lineto] [lon1] [lon2] [dx] [lat1] [lat2] [dy]

### exec

The exec command can be used to execute a PostScript subroutine for each point in the shape.

The command syntax is

    \{moveto} \{lineto} exec [shapefile]

Here {moveto} and {lineto} are PostScript programs to be executed whenever a PostScript moveto or lineto command would be produced by the same. The most common use for the command is drawing circles at populated places like this:

    { gsave translate
      1 setlinewidth
      2 0 m 2 2 l -2 2 l -2 -2 l 2 -2 l closepath
      gsave 1 0 0 setrgbcolor fill grestore
      0 setgray stroke
      grestore
    }
    dup
    exec [shapefilename]

### qdexec

The qdexec command can be used to execute a PostScript subroutine for each grid point in the given querydata.

The command syntax is

    {program} qdexec [querydata]

The command can be used for example to render a small marker at each grid point.

### project

The command syntax is

    project [longitude] [latitude]

The command converts the given coordinates into the respective PostScript image coordinates, and leaves the numbers on the PostScript stack. The user can then for example draw a marker at the location, or render a string at the location like this:

    /Helvetica findfont 10 scalefont setfont
    project 25 60 moveto (Helsinki) show

### location

The command syntax is

    location [locationname]

The command is similar to the project command, the location is just identified by name instead of its coordinates.

### system

The command syntax is

    system [any sequence of words]

The program will run the remainder of the line as a system command, and inserts the output into the PostScript. For example, one may put the current date into the image like this:

    /Helvetica-Bold findfont 10 scalefont setfont
    5 5 moveto
    system date +"(%d.%m.%Y)"
    show

### querydata

The command syntax is

    querydata [filename]

The command is used to set the currently active querydata for querydata rendering commands.

### parameter

The commands syntax is

    parameter [paramname]

The command is used to set the active parameter in the current querydata.

### level

The command syntax is

    level [levelvalue]

The command is used to set the active level in the querydata. The command is most commonly used when rendering images out of pressure level data.

### timemode

Set the timemode to utc or local time. The command syntax is

    timemode [utc|local]

### time

Set the time to be selected out of the current querydata. The command syntax is

    time [days] [hour]

The days argument how many days forward to proceed from the start of the querydata,
and the hour argument which hour to set active for that day.

### smoother

The command syntax is

    smoother [type] [factor] [radius]

The command is used to smoothen the querydata before contours are calculated from it.
Currently the only good smoother type is PseudoGaussian. The value for factor should range somewhere from 1 up to 20-30, with higher values meaning less smoothing. The radius should be adapted to the data. Usually the value should be atleast twice the value of the grid spacing, maybe even 3. Any higher values will slow down the smoother rapidly.

### contourcommands

The command syntax is

    contourcommands [moveto] [lineto] [closepath]

The command is used to specify the aliases for the contours produced when contouring querydata.

### contourline

The command syntax is

    contourline [value]

A line will be produced from the querydata going through the given value.

### contourfill

The command syntax is

    contourfill [lolimit] [hilimit]

A polygon will be produced from the querydata encompassing the areas with the given values. Either limit may be substituted with the "-" character to get an open ended interval.

### bezier

The bezier command is used to specify what type of Bezier fitting will be done to the querydata contours, if any.
This will turn Bezier fitting off:

    bezier none

This will try to fit a Bezier curve exactly to the path:

    bezier cardinal [0-1]

where the given argument is a tolerance factor. This command will settle for an approximate fit:

    bezier approximate [maxerror]

where maxerror measures the allowed maximum error at node points measured in PostScript points. This will produce a tighter fit:

    bezier tight [maxerror]
