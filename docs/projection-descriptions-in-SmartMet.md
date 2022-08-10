* [Smartmet tools projections](#smartmet-tools-projections)
  * [Definition of the projection](#definition-of-the-projection)
    * [Plate Carrée (latlon)](#plate-carrée-latlon)
    * [Rotated plate Carrée (rotated latlon)](#rotated-plate-carrée-rotated-latlon)
    * [Inverse rotated plate Carrée](#inverse-rotated-plate-carrée)
    * [Mercator](#mercator)
    * [Orthographic](#orthographic)
    * [Stereographic](#stereographic)
    * [Gnomonic](#gnomonic)
    * [Equidistant](#equidistant)
    * [YKJ](#ykj)
  * [Definition of the geographic area](#definition-of-the-geographic-area)
    * [Corner coordinates](#corner-coordinates)
    * [Center coordinate plus scale](#center-coordinate-plus-scale)
  * [Definition of the grid](#definition-of-the-grid)

## Smartmet tools projections

Smartmet tools use a string to define the grid used. It consists of the following parts:

  1. definition of the projection
  2. definition of the geographic area
  3. definition of the grid

Smartmet tools use a spherical earth with a radius of 6371.2 kilometers, hence no ellipsoid needs to be defined. Different radii can be usually be used without problems unless actual distance calculations are done. For example, if one visualizes data using the exact same corner coordinates, the end result will be the same even though the radii were different.

The different parts are separated by a colon:

    projection:area:grid

### Definition of the projection

The projection is defined by the name of the projection as used by the Smartmet tools, followed by a comma separated list of optional parameters:

    name
    name,param1
    name,param1,param2
    ...

The order of the parameters is fixed. Hence if one wishes to redefine only param2, one must also give a value for param1.

#### Plate Carrée (latlon)

Since the latitude and longitude as mapped as is to the X- and Y-coordinates, there are no extra parameters. The description is thus of the form

    latlon:area:grid

#### Rotated plate Carrée (rotated latlon)

A rotated latlon area is commonly used in meteorological models to get the grid spacing more even near the center of the calculation area. The rotated area is defined by defining the rotated south pole of the area:

    latlon,lat_0,lon_0:area:grid

The default values are:

* lat_0 = -90
* lon_0 = 0

#### Inverse rotated plate Carrée

An inversely rotated latlon area is essentially the same the rotated latlon area. It differs in only how the area corner coordinates in the second part of the definition are to be interpreted. In the first they are interpreted to be the coordinates in the regular latlon world normally used in cartography. In the inverse version they are interpreted to be the corner coordinates in the already rotated coordinate system.

The form of the definition is thus identical:

    latlon,lat_0,lon_0:area:grid

The default values are:

   * central latitude lat_0 = -90
   * central longitude lon_0 = 0

#### Mercator

The Mercator projection takes no parameters, hence the description is of the form

    mercator:area:grid

#### Orthographic

The orthographic projection takes a single parameter: the azimuth, whose default value is 0.

    orthographic,azimuth:area:grid

#### Stereographic

The stereographic projection definition is of the form

    stereographic,lon_0,lat_0,lat_ts:area:grid

The default values are:

* central longitude lon_0 = 0
* central latitude lat_0 = 90
* true latitude lat_ts = 60

#### Gnomonic

The gnomonic projection definition is of the form

    gnomonic,lon_0,lat_0,lat_ts:area:grid

The default values are:

* central longitude lon_0 = 0
* central latitude lat_0 = 90
* true latitude lat_ts = 60

#### Equidistant

The equidistant projection definition is of the form

    equidist,lon_0,lat_0:area:grid

The default values are:

* central longitude lon_0 = 0
* central latitude lat_0 = 90

#### YKJ

YKJ is the old "Yhtenäiskoordinaatisto" system used in Finland. It takes no parameters.

### Definition of the geographic area

The area can be defined in two different ways, the latter with an optional modifier.

#### Corner coordinates

In this form the area is defined by listing the bottom left and upper right corner point coordinates as follows:

    lon_bl,lat_bl,lon_ur,lat_ur

where bl stands for bottom left and ul for upper right respectively.

#### Center coordinate plus scale

In this form the area is defined by giving the center coordinate plus a scale factor. By default the aspect ratio, the ratio of distances in y- and x-coordinates, is assumed to be one. It can be modified if necessary:

    lon_0,lat_0,scale
    lon_0,lat_0,scale/aspect

In general it is easier to customize areas defined using the central point than corner points, since one is then free to choose image width, height and scale without distorting the aspect ratio.

### Definition of the grid

The grid definition merely defines the resolution of the data, once the projection and the area have been defined. The definition can be of two forms:

    width,height
    x1,y1,x2,y2

The first form is by far the most common. In some applications the grid size is irrelevant, and the part can be omitted completely. In that case the grid definition is assumed to be equivalent to

    0,0,1,1