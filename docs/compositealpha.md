compositealpha is a tool for creating images with partial transparency more easily. The most common use is to have a regular map, and combine it with a black and white image where black marks the parts of the image that will not be transparent. compositealpha will then create a transparent image where only parts of the map will be visible. Such maps are commonly used when creating animations as the foreground image to be placed on top of all the weather data. Otherwise there might be no map features visible on the image to help identify locations.

## Command line

The command line syntax is

    compositealpha image mask resultimage

There are no command line options.

## Examples

Examples of compositealpha usage can be found from directory (see Makefile.common)

    /smartmet/share/maps/default

The compositealpha command is called from Makefile.common:

    compositealpha background.png mask.png foreground.png

The background is a regular map of the Latvia:

[[images/background.png]]

And the mask is used to mark the parts we wish to render on top of all weather data:

[[images/mask.png]]

And this is the final result:

[[images/foreground.png]]