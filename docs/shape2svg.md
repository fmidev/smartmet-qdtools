shape2svg is used to generate SVG paths used in textgen production. The output is not pure SVG as it contains only area paths.

## Usage

    shape2svg [options] shapename

## Options

* **-f** **<****name****>**  
    The name of the attribute from which the filename is generated (default = NAME). Has to be a string.
* **-d** **<****dir****>**  
    The output directory (default = .)

## Example

Named polygons in the shape can be investigated with [QGis](http://qgis.org/) or [shape2xml](shape2xml.md) programs.

For example command

    shape2xml /smartmet/share/gis/shapes/Latvia/regions |  grep "<shape"

shows that regions shape contains following polygons:

    <shape id="0" type="5" NAME="Kurzeme">
    <shape id="1" type="5" NAME="Latgale">
    <shape id="2" type="5" NAME="Lielriga">
    <shape id="3" type="5" NAME="Vidzeme">
    <shape id="4" type="5" NAME="Zemgale">

Then polygon can be saved to SVG paths with a command

    shape2svg -f NAME regions

Then all polygons having attribute NAME are extracted to their own file.

    Kurzeme.svg
    Latgale.svg
    Lielriga.svg
    Vidzeme.svg
    Zemgale.svg
