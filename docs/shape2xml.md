shape2xml is a simple tool for converting shape files to XML. The output XML contains shape polygons as SVG paths with an attribute information. The tool is handy to investigate content of shape file.

Usage:

    shape2xml [shapefile]

Example:

    shape2xml /smartmet/share/gis/shapes/Latvia/regions |  grep "<shape"

output:

    <shape id="0" type="5" NAME="Kurzeme">
    <shape id="1" type="5" NAME="Latgale">
    <shape id="2" type="5" NAME="Lielriga">
    <shape id="3" type="5" NAME="Vidzeme">
    <shape id="4" type="5" NAME="Zemgale">

