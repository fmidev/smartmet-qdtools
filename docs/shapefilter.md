shapefilter is used to extract a subset of shapedata from an ESRI shapefile.

## Command line

The command line syntax is

    shapefilter [options] inputshape outputshape

The shape parameters are given without any trailing .shp suffix.

The available options are

* **-f name=value**  
    Keep only the elements whose attributes match the desired value
* **-e**  
    Keep only edges that appear an even number of times
* **-o**  
    Keep only edges that appear an odd number of times
* **-b lon1,lat1,lon2,lat2**  
    Keep only shapes which appear atleast partly in the given bounding box. The bounding box is identified by the coordinates of the bottom left and top right corners.

## Examples

With the qgis program we can see that in the shape /smartmet/share/gis/shapes/Latvia/regions.shp there is region with attribute NAME and value Lielriga. That region can be extrapolated to its own shape with a command:

    shapefilter -f NAME=Lielriga regions lielriga

In country shapes all individual countries are stored as separate polygons. This means that the common border of countries will appear twice in the shapefile. This can be utilized to extract the common borders from the shape with

    shapefilter -e country borders

since the common borders necessarily appear an even number of times.

Conversely, using the command

    shapefilter -o country shoreline

we can extract only the shorelines, not the national boundaries on land.
