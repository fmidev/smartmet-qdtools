qdarea is a program for extracting weather information over user defined areas in querydata. The program can be used for example for calculating daily maximum temperatures at some point, within some radius around a point, or even inside generic polygon.

This simple tutorial shows how to retrieve maximum wind and mean temperature from the lake Usmas ezers. First, the tutorial shows how to crop lake from larger shape file. Then the cropped shape is converted to SVG path, which is finally used by qdarea to retrieve data.

Related manuals:

* [qdarea](qdarea.md)
* [shapefilter](shapefilter.md)
* [shape2svg](shape2svg.md)

See also:

* [Server directory structure](server-directory-structure.md)

Table of contents:

* [Crop Area from Larger Shape File](#crop-area-from-larger-shape-file)
* [Convert Shape to SVG Path](#convert-shape-to-svg-path)
* [Get Data from the Area](#get-data-from-the-area)

## Crop Area from Larger Shape File

First, we need to find convenient data. Shape files are stored in directory

    /smartmet/share/gis/shapes

Free program [QGis](http://qgis.org/) helps to investigate the data.

Here, shape /smartmet/share/gis/shapes/ESRI/europe/water.shp is used. With a help of information tool of QGis, we can see, that the polygons in the shape have attribute NAME.

The polygon can be cropped from the shape file with shapefilter program with following commands:

    mkdir -p /tmp/shapes
    shapefilter -f NAME="Usmas ezers" /smartmet/share/gis/shapes/ESRI/europe/water /tmp/shape/usmas_ezers

Note that .shp postfixes are omitted.

## Convert Shape to SVG Path

Next, the shape need to be converted to SVG path. It is a very simple one command operation. The SVG paths are stored in directory

    /smartmet/share/paths

This path will be saved into directory /smartmet/share/paths/lakes. So the command is:

    shape2svg -d /smartmet/share/paths/lakes/ /tmp/usmas_ezers.shp

After executing the command, file Usmas ezers.svg appears to the directory.

Note, that the file is named according the polygon name and thus the file name contains space. It is normally a good idea to avoid special characters in filenames and move the file to a new name:

    mv /smartmet/share/paths/lakes/Usmas\ ezers.svg /smartmet/share/paths/lakes/usmas_ezers.svg

Note also, that cropping polygon from larger shape is not mandatory. The same result could have achieved with a command:

    shape2svg -f NAME="Usmas ezers" -d /smartmet/share/paths/lakes/ /tmp/usmas_ezers.shp

It is still often a good idea to crop the shape first so that it can be checked with QGis.

## Get Data from the Area

Finally, it is time to retrieve data to the specified area. Let us assume that we want maximum wind and mean temperature with six hours interval.

Without more explanations, that happens with a command below. Please consult qdarea manual for more details.

    qdarea -p /smartmet/share/paths/lakes/usmas_ezers.svg:10 -T 6 -P 'max(wspd),mean(wdir),mean(t2m)' -q /smartmet/data/ecmwf/europe/surface/querydata

Note, that since Usmas ezers is quite a small lake, no grid point of any available data hits on the area. Thus, the area have to be expanded with syntax -p /smartmet/share/paths/lakes/usmas_ezers.svg:10. The idea of calculating mean and max values to the region instead of point kind of dissipates here, but the idea of the process remains. Unfortunately there was no convenient shape data to create SVG from a larger region.

Note also, that although maximum wind speed is searched from the data, maximum wind direction does not make sense. Instead mean wind direction is used.

Note that, parameter names have aliases in qdarea. Parameter names used by qdpoint work as well.

Output of the command looks something like:

    /smartmet/share/paths/lakes/usmas_ezers.svg:10 201110260600 201110261200 6.1 123.3 4.3
    /smartmet/share/paths/lakes/usmas_ezers.svg:10 201110261200 201110261800 6.1 133.3 7.1
    [...]
    /smartmet/share/paths/lakes/usmas_ezers.svg:10 201111041200 201111041800 9.5 260.0 9.1
    /smartmet/share/paths/lakes/usmas_ezers.svg:10 201111041800 201111050000 7.8 250.0 7.5

Where the first column is area, second start time of the time sequence, third end time and then parameters in the same order they were asked.
