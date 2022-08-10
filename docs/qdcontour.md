The qdcontour program takes as input a control file, and depending on the contents of the control file renders either contour images from querydata, or maps from shapefiles.

Table of contents: 
* [qdcontour versions](#qdcontour-versions)
  * [Comments](#comments)
  * [Variables](#variables)
  * [Flow control](#flow-control)
* [Rendering shapefiles](#rendering-shapefiles)
  * [Specifying the projection](#specifying-the-projection)
  * [Drawing a single shapefile](#drawing-a-single-shapefile)
  * [Specifying the graphics format](#specifying-the-graphics-format)
  * [Saving the result](#saving-the-result)
* [Rendering contours](#rendering-contours)
  * [Rendering meridians](#rendering-meridians)
  * [Querydata control](#querydata-control)
  * [Rendering many queryfiles](#rendering-many-queryfiles)
  * [Controlling generated image filenames](#controlling-generated-image-filenames)
  * [Controlling the contours](#controlling-the-contours)
  * [Font control](#font-control)
  * [Drawing symbols at grid points](#drawing-symbols-at-grid-points)
  * [Drawing high and low pressure symbols](#drawing-high-and-low-pressure-symbols)
  * [Drawing arrows from querydata](#drawing-arrows-from-querydata)
    * [Customizing round arrows](#customizing-round-arrows)
  * [Placing text values in the images](#placing-text-values-in-the-images)
  * [Masking out labels and arrows](#masking-out-labels-and-arrows)
  * [Saving the results](#saving-the-results)
  * [Caching contours for speed](#caching-contours-for-speed)
  * [Interpolation of the querydata](#interpolation-of-the-querydata)
  * [Filtering the querydata](#filtering-the-querydata)
  * [Extrapolating querydata](#extrapolating-querydata)
  * [Smoothening the querydata](#smoothening-the-querydata)
  * [Reducing noise in the querydata](#reducing-noise-in-the-querydata)
  * [Selecting the time zone](#selecting-the-time-zone)
  * [Placing timestamps in the images](#placing-the-timestamps-in-the-images)
  * [Automatically calculated metaparameters](#automatically-calculated-metaparameters)
  * [Unit conversions](#unit-conversions)
* [Appendix](#appendix)
  * [Environment variables](#environment-variables)
  * [Color](#color)
  * [Blending colors](#blending-colors)
  * [A sample contouring control file](#a-sample-contouring-control-file)
  * [Deprecated features](#deprecated-features)

## qdcontour versions

Currently FMI supports two different versions of qdcontour

   * qdcontour is the classic version of the program
   * qdcontour2 uses Cairo as a graphics rendering backend

The main differences between the versions are as follows

*    The newer Cairo version is able to produce higher quality looking results than the classic version. It can also produce vector graphics output in SVG, EPS and PDF formats. However, whether the output is pure vector graphics or partially or fully rasterized depends on the capabilities of the Cairo backend. For example, EPS does not support transparency, and any such part in the image may be rasterized to emulate the appearance.
*    qdcontour and qdcontour2 also define fonts differently. With qdcontour you referred directly to the font name as it appears in system font paths. With the Cairo backend you refer only to the name of the font as it is seen by the operating system.
*    qdcontour2 supports arrowlinewidth command, which is especially useful for fine tuning the appearance of meteorological wind arrows

In the following all documentation is common for qdcontour and qdcontour2 unless otherwise mentioned.

**Command line options**  

The program is used as follows:

    qdcontour [-v] [-f] [-q querydata] [controlfile]

The options are

    -v
        Verbose output. For example output files are printed.
    -f
        Force output. Normally qdcontour may decide not to draw some contour image since an image by
        that name already exists. This greatly speeds up contouring by omitting unnecessary redrawing.
        However, occasionally one changes the control file somehow, and a redraw must be done by using
        the
    -f option.
    -q querydata
        Use the given querydata file as if the respective "querydata" command was given in the control file.
 
    -c "config line"
        qdcontour2 only. Useful for mainly temporarily changing the some default setting. For example -c "format pdf".

**Control file syntax**

### Comments

Normal comments of the following form are supported.

    # comment
    // comment
    command # comment
    command // comment

### Variables

Variables are defined following:

    #define $paramname value

And used following:

    $paramname

### Flow control

The control file parser provides for very little flow control, in particular there are no loop structures.

    include [filename]

## Rendering shapefiles

### Specifying the projection

See also [Projection descriptions in Smartmet](projection-descriptions-in-SmartMet.md)

Before an image containing possibly multiple individual shapefiles can be rendered, one must specify the projection and the bounding box of the area to be rendered. The command to define the projection is

    projection [description]

The description is of the form

    [projectiondetals]:[areadetails]:[xycoordinates]

where projection details is one of:

*    latlon
*    ykj
*    kkj
*    mercator
*    rotlatlon,polelatitude=-90,polelongitude=0
*    stereographic,centrallongitude=0,centrallatitude=90,truelatitude=60
*    gnomonic,centrallongitude=0,centrallatitude=90,truelatitude=60
*    equidist,centrallongitude=0,centrallatitude=90

Default values are indicated when omitting a particular value is possible.

The area details is of one of the following forms

    x1,y1,x2,y2
    cx,cy,scale

where the first form requires the bottom left and top right longitude and latitude. The latter form requires the center

The possible values for the xy-coordinates are

    x1,y1,x2,y2
    x2,y2

In qdcontour one usually uses only the latter form, since only the size of the image is of interest. If either coordinate is negative, qdcontour will recalculate the value so that aspect ratio is preserved. Naturally this means both x2 and y2 cannot be negative at the same time.

### Drawing a single shapefile

There are two ways to render a shape file, either by filling or stroking polyline or polygon data, or by placing markers at point data.

Polyline or polygon data rendering is controlled using the command

    shape [shapefilename] [fillcolor] [strokecolor]

where shapefilename is the name of the shape, without any suffix such as .shp or .dbf. If the filename cannot be found directly, the environmental imagine::shapes_path is used to locate the file.

The fillcolor and strokecolor can be in the form given in Color. Often one of the colors is specified to be none, indicating that either filling or stroking is not to be done.

Point data rendering is controlled using the command

    shape [shapefilename] mark [markerfilename] [blendingrule] [blendfactor]

where markefilename is the image to be used as a marker. The blending rules are explained in section Blending colors, the blendfactor can be used as an extra alpha factor in the range 0-1, but often one uses the value 1 to indicate no extra blending is to be done.

### Specifying the graphics format

Before actually rending the final image, once should choose the desired graphics format and adjust the parameters related to the format as desired.

The available commands are

* **format [format]**  
    where [format] may be one of png, jpg or gif. The default format is png. qdcontour2 supports also formats svg, eps and pdf.
* **savealpha [0-1|0-1]**  
    is used to choose whether the alpha channel of the rendered image should be saved or ignored. Typically on ignores the alpha channel, unless the image intentionally contains transparent areas. The default is to save alpha channel.
* **reducecolors [0-1|0-1]**  
    If set to 1, the number of colors in the image will be adaptively reduced until the error is small enough. This is often used with the wantpalette option.
* **wantpalette [0-1|0-1]**  
    This option is relevant only to the png format, which is the only one that allows both palette and truecolor storage. The flag indicates whether qdcontour should try to store the image in palette mode, if it can. For png this means there can be atmost 256 different colors, including differences in alpha.
* **forcepalette [0-1|0-1]**  
    The intent is to enable forcing a palette save for png format. However, this has not been implemented yet.
* **jpegquality [0-100|0-100]**
    This allows one to control the quality of the compressed jpeg image. The higher the number, the better the quality, but also the larger the image file will be. The default value is -1, which implies the default chosen by libjpeg is used.
* **pngquality [1-9|1-9]**  
    This allows one to control the compression level of the png format. The default value is -1, which implies the default chosen by libpng is used.
* **alphalimit [-1-127|-1-127]**  
    Alphalimit enables one to enforce binary transparency, that is, alpha is either on or off. The actual values depend on the chosen image format. The value -1 implies binary transparency is not enforced. This value is most relevant for the gif format, which does not support full transparency.

Perhaps the set of most typical image format related commands is

    format png
    savealpha 0
    reducecolors 1
    wantpalette 1

### Saving the result

Once the projection and the shapefile rendering directives have been given, the actual output can be generated using the command

    erase [color]
    draw shapes [filename]
    clear sizes
    clear shapes

which causes all the shapefiles specified earlier to be drawn in the given filename. The suffix of the filename is chosen automatically according to the active graphics format.

The erase command essentially defines the background color for the image. The default is to use transparent color, which most likely is not suitable for most maps.

The final three clearing commands do not have to be given, but act as safety in case somebody intends to draw more shapes in the same control file.

    qdcontour draws data in the following order:

    1. background image (or fixed colour)
    2. for each parameter in the order they are listed in the config:
    a) contour fills
    b) contour patterns
    c) contour lines
    d) overlay image
    3. graticule
    4. foreground image
    5. wind arrows
    6. contour symbols
    7. contour fonts
    8. contour labels
    9. generic labels
    10. pressure symbols
    11. final overlay defined with the combine command

## Rendering contours

Note that many commands have effect only on the last parameter defined. If no parameter has been defined yet, the commands may appear to have no effect at all. If a command is parameter specific, it will be mentioned in the documentation for that command.

### Rendering meridians

The meridians may be rendered using the command

    graticule [lon1] [lon2] [dx] [lat1] [lat2] [dy] [color]

and disabled with

    clear graticule

For example, one may cover Scandinavia with

    graticule 0 40 5 50 80 2 black

### Querydata control

One may specify the querydata files to be used with the command

    querydata [filename1,filename2,_.,filenameN|filename1,filename2,_.,filenameN]

If the filenames are not absolute, and cannot be found at the given places, the smartmet.conf setting qdcontour::querydata_path is used for searching for the file. As a special case, if the given name is a directory, the newest querydata in that directory is used. The time range one is able to contour is determined by the common time span of the given queryfiles.

Whenever a parameter is specified to be contoured, the order of the queryfiles is the order in which the parameter is searched.

The desired level value may be controlled with

    level [levelvalue]

The default level value is -1, indicating the first level of the data. The command has an effect only on the active parameter.

### Rendering many queryfiles

In general one cannot render multiple queryfiles simultaneously. However, one may simulate this behaviour by dropping the "querydata" command from the control file, and using the command line option "-q" instead. For example,

    find /data/pal/querydata/tuliset/suomi/dbz_osat/ \-name '*.sqd' \-exec qdcontour \-q \{\} \;

or

    DIR=/data/pal/querydata/tuliset/suomi/dbz_osat
    for FILE in `ls \-1 $DIR`; do
    qdcontour \-q $DIR/$FILE
    done

### Controlling generated image filenames

As qdcontour goes through the available times in the specified querydata, it will attempt to determine an unique filename for each image. By default each filename is of the form

    [prefix][timestamp][origintimestamp][suffix].[format]

The format is uniquely determined by using the command

    format [format]

where format is one of png, jpeg, gif or wbmp.

The prefix and suffix are by default null strings, but can be modified with the commands

    prefix [prefix]
    suffix [suffix]

The origintimestamp is the time stamp string generated from the origin time of the query data, and is intended for distinguishing between various forecast times. Whether or not the string is used can be modified with the command

    timestamp [0-1|0-1]

The format of the tiemstamp can be controlled using the command

    timestampformat value

where value is a sum of the individual fields to be set as described by the NFmiStaticTime class:

    YY = 1
    YYYY = 2
    MM = 4
    DD = 8
    HH = 16
    MM = 32 (minutes)
    SS = 64

The default is YYYYMMDDHHMI, which equals 62.

Finally, the files must be stored in some directory, which is controlled with

    savepath [path]

The default value for path is ".", the current directory.

For example, when contouring T2m forecasts one might use

    prefix ENN_
    suffix _T2M
    timestamp 1
    savepath /foo/bar

### Controlling the contours

One is able to contour several parameters simultaneously. The specifications for each individual parameter are started by the command

    param [parametername]

To draw contour lines on the parameter one can then repeatedly use

    strokerule [blendrule]
    contourline [value] [color]

or define multiple lines simultaneously with

    contourlines [startvalue] [endvalue] [step] [startcolor] [endcolor]

The commands have an effect only on the active parameter.

The colour for each individual line is interpolated linearly between startcolor and endcolor in the HSV color space, which is more likely suitable in this case than direct RGB interpolation.

One should remember, that the latest defined strokerule is the one to be used for each contourline.

Alternatively, one may specify some values intervals to be filled with the desired colors by repeatedly applying the command

    fillrule [blendrule]
    contourfill [startvalue] [endvalue] [color]

As special cases, if startvalue is "-", it is interpreted to mean minus infinity. Similarly endvalue would be interpreted to be plus infinity. However, since the case

    contourfill - - [color]

makes rarely sense, we define it instead to mean colouring all missing values in the data. This is especially useful for data which has limited range, such as radar data.

The command has an effect only on the active parameter.

The width of the contour lines can be controlled with

    contourlinewidth [value]

Values less than 1 or large values are not recommended since the rendering implementation is of low quality.

Also, one may define multiple fills simultaneously with

    contourfills [startvalue] [endvalue] [step] [startcolor] [endcolor]

which works similarly to the contourlines command documented earlier.

The command has an effect only on the active parameter.

As with the case of strokerule, the latest defined blendrule is the one to be used for each contourfill command.

Finally, one may use an image pattern instead of a fixed color. This is accomplished using the command

    contourpattern [startvalue] [endvalue] [patternfile] [blendrule] [blendfactor]

where patternfile is the path to the image file containing the pattern to be used when filling. The filling is performed using blendrule with the extra blendfactor alpha factor.

The command has an effect only on the active parameter.

There is no similar patternfills equivalent as before, as interpolation between the start and end patterns would rarely make sense.

### Font control

With qdcontour one has to be quite specific on the font to be used. The common syntax for specifying the font is

    featurenamefont name:[width]x[height]

Often one uses a fixed size font which is guaranteed to give reasonably good results, as in "misc/5x8.pcf.gz:5x8".

qdcontour2 uses the system font manager instead, and the common syntax for specifying the font is changed to

    featurenamefont name:size

For example: "Arial-Narrow:10".

In RedHat Linux new fonts can be added to the system by copying the files to directory /usr/share/fonts and then running the fc-cache command.

For example, FMI has licensed some Adobe TrueType fonts for internal use, and placed them into the /usr/share/fonts/adobefonts directory:

    AdLibBT-Regular.ttf
    AmazoneBT-Regular.ttf
    AmeliaBT-Regular.ttf
    AmericanaBT-Bold.ttf
    AmericanaBT-ExtraBold.ttf
    AmericanaBT-Italic.ttf
    AmericanaBT-Roman.ttf
    AmericanGaramondBT-BoldItalic.ttf
    AmericanGaramondBT-Bold.ttf
    AmericanGaramondBT-Italic.ttf
    AmericanGaramondBT-Roman.ttf
    AmericanTextBT-Regular.ttf
    AmerigoBT-BoldA.ttf
    AmerigoBT-BoldItalicA.ttf
    AmerigoBT-ItalicA.ttf
    AmerigoBT-MediumA.ttf
    AmerigoBT-MediumItalicA.ttf
    AmerigoBT-RomanA.ttf
    AndaleMono.ttf
    Arial-Black.ttf
    Arial-BoldItalicMT.ttf
    Arial-BoldMT.ttf
    Arial-ItalicMT.ttf
    ArialMT.ttf
    ArialNarrow-BoldItalic.ttf
    ArialNarrow-Bold.ttf
    ArialNarrow-Italic.ttf
    ArialNarrow.ttf
    BakerSignetBT-Roman.ttf
    ...

**Labeling the contours**  

To label certain contours one may use the commands
    
    contourlabel [value]
    contourlabels [startvalue] [endvalue] [step]

The commands have an effect only on the active parameter.

If one may want to label contour with text following syntax is convenience

    contourlabel [value]
    contourlabeltext [value] [text]

The relevant settings may be controlled with

    contourlabelfont [font]         # default = misc/5x8.pcf.gz:5x8
    contourlabelcolor [color]       # default = black
    contourlabelbackground [color]  # default = #20B4B4B4
    contourlabelmargin [dx] [dy]    # default = 2 2
 
    contourlabelmindistsamevalue [value]            # default = 200
    contourlabelmindistdifferentvalue [value]       # default = 30
    contourlabelmindistdifferentparam [value]       # default = 30

The commands have an effect only on the active parameter.

Note that contour labels are not effected by any foreground or mask.

### Drawing symbols at grid points

Often for example thunder appears in such as small area that any attempt of using contourpattern for rendering the area is likely to fail. An alternative is to draw a symbol at any grid point where the value is in the desired range.

The syntax for the command is

    contoursymbol [startvalue] [endvalue] [imagefile] [blendrule] [blendfactor]

The command has an effect only on the active parameter.

The placement of the images can be controlled with

    contoursymbolmindist [value] \# default = 4 pixels

Alternatively, one may use a specific symbolic font to draw the symbols. The syntax is

    contourfont [value] [charnum] [color] [font]

The command has an effect only on the active parameter.

The placement of the symbols can be controlled with

    contourfontmindistdifferentparam [value] \# default = 8 pixels
    contourfontmindistdifferentvalue [value] \# default = 8 pixels
    contourfontmindistsamevalue [value] \# default = 8 pixels

### Drawing high and low pressure symbols

    highpressure [image] [blendrule] [blendfactor]
    lowpressure [image] [blendrule] [blendfactor]
 
    lowpressuremaximum [value]              # default 1020
    highpressureminimum [value]             # default 980
 
    pressuremindistsame [value]             # default = 50
    pressuremindistdifferent [value]        # default = 50
 
    clear pressure

### Drawing arrows from querydata

One may choose which parameters will be used as a direction - speed pair when rendering arrows using the commands

    directionparam [parametername]
    speedparam [parametername]

or alternatively with

    speedcomponents parametername U parametername V

Note that U- and V-component up-direction is assumed to be grid north, not true north as when direction and speed is used.

The default values are WindDirection and WindSpeedMS respectively.

The arrow to be drawn at each location is defined with the commands

    arrowpath [filename]
    arrowpath meteorological
    arrowpath roundarrow

where filename contains an SVG-style (limited) path definition, which will be rotated according to the direction parameter. Specifying meteorological arrow will let qdcontour automatically calculate the suitable meteorological arrow. The arrow does not at the moment fully comply with the specifications, no flag is generated from speeds above 25 m/s. The roundarrowtype consists of a circle with a triangle pointing at the correct direction.

The arrow is rendered (except roundarrow, see the end of this section) according to the rules specified by

    arrowfill [color] [blendrule]
    arrowstroke [color] [blendrule]
    arrowfill [lolimit] [hilimit] [color] [blendrule]
    arrowstroke [lolimit] [hilimit] [color] [blendrule]
 
    arrowlinewidth [width]   # qdcontour2 only

Meteorological wind arrows are only stroked, not filled. Using the generic setting without limits causes all range specific settings to be erased. One may set the default colour using the generic version, and then make range specific exceptions.

The size of the arrow is principally determined by
    
    arrowscale [scalefactor]

whose default value is 1.0. The factor is used to scale the arrow path definition uniformly. Additionally, one may also use the command

    windarrowscale [A] [B] [C]

to scale the arrow nonlinearly and depending on the speed at the location. The default values for a,b and c are 0,0 and 1 respectively, and the formula applied to calculate an extra scaling factor for the arrow is

    A log (BS + 1) + C

where S is the speed value. Note that the value is always 1 for the default parameters, and no extra scaling thus occurs. A is used to control linearly how fast the arrow grows, while B is used to control it logarithmically. In general one should try to find a suitable value for B first, to represent the speed of increasing the size of the arrow, and then scale using A to find a suitable final size.

The arrows themselves are put on the image using the commands

    windarrow [longitude] [latitude]
    windarrows [dx] [dy]
    windarrowsxy [x0] [y0] [dx] [dy]

The first form places the arrow at a specific location. Both the direction and the speed will be interpolated, if necessary, for the given coordinates.

The second form places an arrow at each grid location, with the given steps. Using dx=1 and dy=1 an arrow would be placed at every grid point. Note that fractional steps are allowed, for example

    windarrows 0.5 0.5

would effectively double the grid resolution.

The third form places the first wind arrow at pixel coordinates x0,y0 and then uses the pixel offsets dx,dy to create a lattice of wind arrows to cover the entire image. The last wind arrows to be drawn are those which are the first ones outside the image. This guarantees an uniform appearance in case the last arrows are very close to the edge.

Wind arrows are drawn after all contours have been rendered.

#### Customizing round arrows

Round arrow rendering is more customizable. The fill, stroke and size attributes can be adjusted for any individual range using

    roundarrowfill [lo] [hi] [circlecolor] [trianglecolor]
    roundarrowstroke [lo] [hi] [circlecolor] [trianglecolor]
    roundarrowsize [lo] [hi] [circleradius] [triangledistance] [trianglewidth] [triangleangle]

The default size values are

    circleradius = 9
    triangledistance = 8
    trianglewidth = 9
    triangleangle = 60

The settings can be reset with

    clear roundarrow

For example, using the settings

    roundarrowfill 0 14 khaki green
    roundarrowfill 14 21 yellow green
    roundarrowfill 21 - red green
    roundarrowstroke - - black black
    roundarrowsize - - 10 12 5 60

one would get differently coloured circles for different speeds, but the stroke and size would be uniform. A more complicated example used in the regression tests is

    roundarrowfill 0 4 lightgreen green
    roundarrowfill 4 7 lightyellow yellow
    roundarrowfill 7 10 lightblue blue
    roundarrowfill 10 - pink red
    roundarrowstroke - - black blue
    roundarrowsize 0 4 7 6 7 60
    roundarrowsize 4 7 9 8 9 60
    roundarrowsize 7 - 12 11 12 60

The result looks like this:

[[images/Roundarrow.png]]

### Placing text values in the images

One can place numeric data values in the image using any one of the commands

    label [longitude] [latitude]
    labelxy [longitude] [latitude] [x] [y]
    labels [dx] [dy]
    labelsxy [x0] [y0] [dx] [dy]

The label command places a label at the specified geographic coordinates. One can place the label at a completely different location in the image using the labelxy command. Finally, one can place labels at the grid points themselves using the labels command. The dx and dy arguments specify the step size in the grid, usually one labels every grid point using a step size of 1.

The labelsxy allows one to draw a lattice of labels based on pixel coordinates just like the windarrowsxy command.

Note that the steps dx and dy can be fractional. For example, using a step 0.5 would effectively double the grid resolution.

The commands have an effect only on the active parameter.

Often the coordinate from which the label value is taken is not convenient for placing the label itself. One can adjust the location of the label sligthly by using

    labeloffset [dx] [dy]

where the adjustment is measured in pixels. However, when one moves the label one may wish to mark the actual geographic spot using

    labelmarker [imagefilename] [blendingrule] [alphafactor]

The commands have an effect only on the active parameter.

The image is placed at all labeled geographic coordinates.

Also, one may place a caption for each label. Usually the caption is the name of the parameter, and is used when several parameters are being drawn.

    labelcaption [captiontext] [dx] [dy] [alignment]

The available alignments are

*    Center
*    North
*    NorthWest
*    West
*    SouthWest
*    South
*    SouthEast
*    East
*    NorthEast

One can modify the appearance of the label itself using the following commands

    labelfont [fontspec]                  (default misc/6x13B.pcf.gz:6x13)
    labelcolor [color]                    (default black)
    labelrule [rule]                      (default OnOpaque)
    labelalign [alignment]                (default Center)
    labelformat [format]                  (default is "%.1f", - implies no output)
    labelangle [angle]                    (default 0)
    labelmissing [label]                  (default is -, disable with none)

The format is the format string of a sprintf command for a floating point value.

The commands have an effect only on the active parameter.

Note that by default there is no format. This is convenient when one only wishes to place markers at the grid points using the labelmarker command as follows

    labelformat -
    labels 1 1
    labelmarker [imagefilename] [blendingrule] [alphafactor]

Finally, one can clear the label location definitions when beginning a new contouring by

    clear labels

### Masking out labels and arrows

Using the commands

    mask [filename]
    mask none

one can use a transparent image to choose whether the label or arrow should be rendered. The decision is made based on the transparency of the pixel nearest to the geographic coordinates from which the label or arrow value is taken. If the pixel is fully transparent, nothing is drawn.

Note that if a geographic coordinate is outside the mask image, it is treated the same way as the mask pixel 0,0. That is, we propagate whatever is at pixel 0,0 outside the mask.

Often one would use the mask command with the same image as the foreground command.

Note: masking works only for labels defined using the "label*" commands, not for contour labels.

### Saving the results

The contoured images can be saved using

    background [filename]
    overlay [paramname] [filename]
    foreground [filename]
    foregroundrule [blendingrule]
    draw contours
    clear contours

The first command may be replaced with an erase color command, provided a foreground is given. Alternatively the foreground may be omitted, provided the background is given.

Normally qdcontour will abort rendering the contours if the querydata being contoured was given as a directory, and the directory has been updated after the querydata was initially read in. The assumption is that a new qdcontour routine has been triggered by the querydata update, and it will replace the images being rendered with newer ones. This can be prevented by using the -f command line option.

The clear contours command is optional, and should only be applied at the end of a file, or when the contour specifications documented in the earlier sections really change. Note especially that whenever one is using labels, the old label parameter is remembered even though clear labels has been used. In such cases qdcontour may become unnecessarily slow as it may smoothen the data only to find out that there is no longer anything to do for that parameter.

One may also modify each output image by overlaying some image on top of them, for example a logo or a legend. This is accomplished using

    combine [imagefilename] [x] [y] [blendingrule] [blendfactor]

The extra image setting can be cleared using

    combine none

Alternatively one may overlay the image once a specific parameter has been processed by using

    overlay [parametername] [imagefile]

The overlay can be cleared with either

    overlay none
    overlay -

### Caching contours for speed

Often one will render the exact same parameters with the exact same contour settings on multiple backgrounds, possibly with a different projections. When the data being contoured is very large, for example radar data, the production is unnecessarily slow since the contours are recalculated for each background.

By using the command

    cache 1

one can turn on an internal cache for all calculated contours. The contours will be stored in an internal hash, whose key is formed by the time, file, parameter etc. information for that particular contour.

However, there is no doubt the cache is not foolproof. For example, if one changes the data smoother between two maps, the cache will not notice it.

As a key rule, if all you change is the backgrounds, foregrounds and the projections, you're safe in using the cache. If not, one should atleast clear the cache before rendering the "new" set of images with

    clear cache

The cache can be turned completely off with the command

    cache 0

### Interpolation of the querydata

One can choose how the querydata is to be interpolated using

    contourinterpolation [Nearest|Linear]

The default value is Linear. Nearest is needed when the data is of discrete nature and cannot be interpolated naturally as floating point numbers.

The commands have an effect only on the active parameter.

There is no Cubic interpolation at the moment. To obtain smoother appearance one should smoothen the data instead, as explained in section Smoothening the querydata

### Filtering the querydata

By default qdcontour renders the first 24 data times in the given querydata. The number of images rendered can be modified using

    timesteps [number]

To skip a specific amount of minutes from the start one can use

    timestepskip [minutes]

This may be useful for example if one wishes to render the first 48 hours at a 1h step, then the rest using a longer step.

Also, one may skip particular times by specifying the time step to be used with

    timestep [minutes]

The default value is 0, implying the time step defined in the data itself will be used.

One may specify the timestep to be rounded the the nearest multiple of the data time using

    timesteprounding [0|1]

Note that one may define a time step which is not a multiple of the time step in the data itself. In such cases one should define how the data values from the new moment in time are to be calculated using the commands

    filter [none|max]
    timeinterval [minutes]

By default the filter is none, and no values can be calculated. The time is then considered invalid, and no image is produced.

A new value can usually be calculated (when interpolation makes sense) using the linear filter, which interpolates the new values linearly from the adjacent existing data times.

Alternatively, one may calculate a new derivative parameter, such as the sum, min or max of the data values. Typically one then chooses the timestep and timeinterval to be equal, although it is strictly speaking not necessary.

For example, to render the daily maximum temperature one could use

    timestep 1440        # 24 hours
    timeinterval 1440
    filter max

Finally, one may explicitly modify the data itself using

    datareplace [sourcevalue] [targetvalue]

This is mostly useful when the original data uses another missing value indicator than 32700, which is used by newbase.

The command has an effect only on the active parameter.

The commands

    datalolimit [value]
    datahilimit [value]

can be used to indicate that any value outside the limited range is to be considered missing data.

The commands have an effect only on the active parameter.

### Extrapolating querydata

Currently qdcontour supports only one way to extrapolate data - to replace missing values by adjacent values. The feature is controlled with the command

    expanddata [0|1]

### Smoothening the querydata

Occasionally the parameter stored in the querydata has too little resolution for contouring purposes. This demonstrates itself in the jaggedness of the rendered contour lines.

To alleviate the problem, qdcontour is able to smoothen the data by calculating a moving weighted average. The commands available for doing this are

    smoother [None|Neighbourhood|PseudoGaussian]
    smootherradius [radius]
    smootherfactor [factor]

The default smoother is None. The recommended smoother when smoothing becomes necessary is PseudoGaussian.

The radius controls the radius of the moving average calculation. Typically uses a radius 2-3 times the grid spacing. The smootherradius is given in meters, in FMI typical value is 100000.

The factor controls the sharpness of the weighting function, the higher the number (say 10-20), the closer the smoothed values are to the originals. A low value such as 1-4 smoothens the data more.

The commands have an effect only on the active parameter.

### Reducing noise in the querydata

One can despeckle the active parameter with the command

    despeckle [lolimit] [hilimit] [radius] [weight] [iterations]

The lolimit and hilimit determine the values that can be despecled. Either limit (or both) can also be set to "-", which implies no limit.

The command has an effect only on the active parameter.

The algorithm used is a weighted median filter. The size of the median filter is given by radius (1-N). Valid weights are in the range 0-100. In a normal median filter the value would be 50. The median filter can be applied several times to further despeckle the image. Normally one would use only one iteration though.

### Selecting the time zone

One can select the time zone for all time stamps using the command

    timestampzone [local|utc]

The default is local time.

### Placing timestamps in the images

One can place a timestamp in the image itself using the commands

    timestampimage [none|forobs]
    timestampimagexy [x] [y]

The first command specifies what kind of a text is placed in the image, the second the coordinates of the text. If the coordinates are negative, they are considered offsets from the bottom or right edges.

The different formats are

* **none**  
    No timestamp is rendered
* **obs**  
    The time of the data is rendered
* **for**  
    The forecast time is rendered
* **forobs**  
    The forecast time is rendered followed by the offset to the data time

The appearance of the timestamp can be controlled with commands

    timestampimageformat [hour|hourdate|datehour|hourdateyear] # default = hourdateyear
    timestampimagefont [font]            # default = misc/6x13B.pcf.gz:6x13
    timestampimagecolor [color]          # default = black
    timestampimagebackground [color]     # default = 185,185,185,185
    timestampimagemargin [dx] [dy]       # default = 2 2

### Automatically calculated metaparameters

The qdcontour program is able to recognize different meta parameters, which correspond to parameter values calculatable from actual parameter values.

The available meta parameters are

* **MetaElevationAngle**  
    The elevation of the sun angle in degrees for each geographic location. This value can be used to render night time areas dark. Note that angle -0.83 is considered dark and angle +0.83 is considered daylight.
* **MetaWindChill**  
    Wind chill index
* **MetaDewDifference**  
    Road temperature minus dew point at 2 meters.
* **MetaDewDifferenceAir**  
    Air temperature minus dew point at 2 meters.
* **MetaT2mAdvection**  
    Temperature advection in degrees per hour
* **MetaThermalFront**  
    Thermal front parameter
* **MetaSnowProb**  
    Probability that precipitation form is snow
* **MetaThetaE**  
    Theta_E

### Unit conversions

Unit conversions can be performed using the command

    units [paramname] [conversiontype]

For example

    units Temperature celsius_to_fahrenheit

Unit conversions can be removed with the command

    clear units

The known conversions are

*    celsius_to_fahrenheit
*    fahrenheit_to_celsius
*    meterspersecond_to_knots
*    meters_to_feet
*    kilometers_to_feet
*    kilometers_to_flightlevel (= feet/100)

## Appendix

### Environment variables

The qdcontour program uses either directly or indirectly the following environment settings in the smartmet.conf file:

* **qdcontour::querydata_path**  
    This specifies the search path for querydata files given in the in the querydata command. The current directory is always searched first though.
* **qdcontour::maps_path**  
    This specifies the search path for the images given in the background, foreground, combine and mask commands. The current default value in Linux is */data/share/maps*.
* **imagine::shapes_path**  
    This specifies the search path for shape files given in the shape command. The current setting for this in Linux is */data/share/shapes*
* **imagine::hershey_path**
    This specifies the search path for Hershey font files. The current setting for this in Linux is */data/share/fonts/hershey*.

### Color

Internally the program represents colors using hexadecimal numbers of form "AARRGGBB" where thr RGB components range from 00 to FF, and the alpha component from 00 to 7F, 7F being transparent. The highest bit is reserved for internal use to represent the "none" color.

The colors understood by the control file parser are

* **none**  
    This is a special color meaning no rendering is to be done.
* **transparent**  
    This is equivalent to color #7f000000
* **colorname**  
    The recognized names are listed in the table below
* **colorname,A**  
    In addition to the color, one may specify alpha in the range 0-127.
* **R,G,B**  
    The individual RGB components in the range 0-255.
* **R,G,B,A**  
    The individual RGB components plus alpha in the range 0-127.
* **#AARRGGBB**  
    The color directly in the hexadecimal form

The recognized colors are those listed in the SVG specification at the W3C site.

### Blending colors

In general, one can define an arbitrary function Blend(color1,color2), which given two colors would produce a result color. The qdcontour program understands several such functions in common use in various graphical packages, and also some in not so common use. Whenever rendering a color (source) on top of another one (destination), qdcontour will store Blend(source,destination) as the new destination color.

First, below are the well known Porter-Duff rules:

* **Clear**  
    Both the color and the alpha of the destination color are cleared. The input colors are not used at all.
* **Copy**  
    The source is copied to the destination. The destination is not used at all.
* **Keep**  
    The destination is kept as is. This is a rather useless blending mode.
* **Over**  
    The source color is composited over the destination using the linear alpha blending. This is the most common blending mode whenever alpha-channels are in use.
* **Under**  
    This is similar to Over, but the roles of the colors are reversed in the formula. In effect, the destination is placed over the source.
* **In**  
    The part of the source lying inside the destination replaces the destination. Typically destination is some kind of a mask for picking a part out of the source.
* **KeepIn**  
    The part of the destination lying inside the source replaces the destination. This is similar to Keep, but with the mask role reversed.
* **Out**  
    The part of the source lying outside of the destination replaces the destination.
* **KeepOut**  
    The part of the destination lying outside of the source replaces the destination.
* **Atop**  
    The part of the source inside the destination replaces the part inside the destination.
* **KeepAtop**  
    The part of the destination inside the source replaces the part inside the source in the destination.
* **Xor**  
    Only non-overlapping areas of source and destination are kept.

In addition, the following extra blending modes are defined:

* **Plus**  
    Add alpha-factored-RGBA values (Fs=1, Fd=1)
* **Minus**  
    Substract alpha-factored RGBA values (Fs=1, Fd=-1)
* **Add**  
    Add RGBA values
* **Substract**  
    Substract RGBA values
* **Multiply**  
    Multiply RGBA values
* **Difference**  
    Absolute difference of RGBA values
* **Bumpmap**  
    Adjust by intensity of source color
* **Dentmap**  
    Adjust by intensity of destination color
* **CopyRed**  
    Copy red component only
* **CopyGreen**  
    Copy green component only
* **CopyBlue**  
    Copy blue component only
* **CopyMatte**  
    Copy opacity only
* **CopyHue**  
    Copy hue component only
* **CopyLightness**  
    Copy light component only
* **CopySaturation**  
    Copy saturation component only
* **KeepMatte**  
    Keep target matte only
* **KeepHue**  
    Keep target hue only
* **KeepLightness**  
    Keep target lightness only
* **KeepSaturation**  
    Keep target saturation only
* **AddContrast**  
    Enhance the contrast of target pixel
* **ReduceContrast**  
    Reduce the contrast of target pixel
* **OnOpaque**  
    Draw on opaque areas only
* **OnTransparent**  
    Draw on transparent areas only

### A sample contouring control file

Below is a sample control file which is used to render T2m contour images for Latvian region (see image below). To get an idea what a typical control file contains, please browse through the example.

[[images/FOR_201110250900_201110210701_T2M.png]]

    # Cache images for speed
    cache 1
 
    # We want all the timesteps in the data. 36 should be enough, max 3 days
    timestep 0
    timesteps 36
 
    # Image format
    savealpha 0
    reducecolors 1
    wantpalette 1
    format png
 
    # Filenames
    timestamp 1
    prefix FOR_
 
    # Timestamp the image
    timestampimage none
 
    # define variable named param with value t2m, the variable is used in
    # maps/t2m.conf (this way same map configuration can be used ofr manu
    # parameters)
 
    #define $param t2m
    suffix _T2M
 
    # interpolate data linearly
    contourinterpolation Linear
 
    # do not smoother data
    smoother None
 
    # blending rules for fill and stroke
    fillrule Copy
    strokerule Over
 
    # parameter to draw, use qdinfo to find out possible parameters
    param Temperature
    contourfill - -50 154,8,117
    contourfill -50 -48 198,26,155
    contourfill -48 -46 230,62,200
    contourfill -46 -44 230,91,251
    contourfill -44 -42 198,77,249
    contourfill -42 -40 164,40,235
    contourfill -40 -38 134,0,212
    contourfill -38 -36 95,13,189
    contourfill -36 -34 107,27,227
    contourfill -34 -32 117,72,250
    contourfill -32 -30 138,121,247
    contourfill -30 -28 4,18,179
    contourfill -28 -26 24,49,214
    contourfill -26 -24 56,104,235
    contourfill -24 -22 111,154,247
    contourfill -22 -20 148,178,242
    contourfill -20 -18 0,80,171
    contourfill -18 -16 6,98,196
    contourfill -16 -14 24,122,219
    contourfill -14 -12 17,141,250
    contourfill -12 -10 64,169,255
    contourfill -10 -8 101,189,247
    contourfill -8 -6 0,103,140
    contourfill -6 -5 2,123,163
    contourfill -5 -4 0,155,186
    contourfill -4 -3 34,188,212
    contourfill -3 -2 103,219,230
    contourfill -2 -1 163,243,247
    contourfill -1 0 212,255,255
    contourfill 0 1 5,179,138
    contourfill 1 2 2,212,149
    contourfill 2 4 138,237,187
    contourfill 4 6 204,255,208
    contourfill 6 8 235,252,207
    contourfill 8 10 235,255,122
    contourfill 10 12 255,234,128
    contourfill 12 14 247,212,35
    contourfill 14 16 245,180,0
    contourfill 16 18 242,149,0
    contourfill 18 20 240,116,0
    contourfill 20 22 255,83,36
    contourfill 22 24 247,23,7
    contourfill 24 26 219,10,7
    contourfill 26 28 189,4,4
    contourfill 28 30 156,2,20
    contourfill 30 32 196,24,98
    contourfill 32 34 232,39,120
    contourfill 34 36 245,88,174
    contourfill 36 38 240,144,216
    contourfill 38 -  247,59,213
 
    # draw temperature numbers starting from pixel coordinates 10,10 with
    # a step of 50 pixels
    labelsxy 10 10 50 50
 
    # draw white pressue lines from 900 hPa to 1100 hPa with 2.5 hPa step
    param Pressure
    contourlines 900 1100 2.5 225,225,225 225,225,225
    # draw labels
    contourlabels 900 1100 5
 
    # maps
    # blending rule for foreground image
    foregroundrule Over
 
    # projection, bounding box and image size configuration
    include /smartmet/share/maps/default/latvia_600x398/area.cnf
 
    # background and foreground information
    background /smartmet/share/maps/default/latvia_600x398/background.png
    foreground /smartmet/share/maps/default/latvia_600x398/foreground.png
    # mask /smartmet/share/maps/default/latvia_600x398/seamask.png
 
    # where output files are saved to
    savepath /smartmet/products/example_animations/$param/latvia_600x398
 
    # command to draw the contours
    draw contours
 
 
    # clear cache information
    clear contours
    cache 0

### Deprecated features
    
    querydatalevel [level]
    contourdepth [depth]
    draw imagemap [fieldname] [filename]
    labelfile [filename]
    labelstroke
    labelfill
    hilimit [value]

