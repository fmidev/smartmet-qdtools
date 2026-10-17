Converts querydata to csv data.

### Usage

    qd2csv [-latlon] <querydata>

The querydata is expected to contain point (station) data. The output is printed to standard output, one row per station and time, times in UTC. Columns containing only missing values are omitted, other missing values are printed as NA.

### Options

* **-latlon**  
    print also the latitude and longitude of each station after the station id
