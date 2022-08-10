qdinterpolatetime interpolates data to desired time resolution (also to lower time resolution despite of name).

Usage:

    qdinterpolatetime timeResInMinutes [startTimeResInMinutes=32700] [maxSearchRangeInMinutes=360 (min)
    [generalInterpolationMethod=1 (=linearly, 5=lagrange)]

For example:

    qdinterpolatetime 60 < input.sqd > output.sqd

Modifies data to one hour time resolution. All parameters, places and levels are processed and reserved.
