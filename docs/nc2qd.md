Converts netCDF files to querydata format.

Note: for CF-conforming NetCDF data, consider using [nctoqd](nctoqd.md) instead, which supports a wider range of options.

### Usage

    nc2qd [options] nc_in qd_out

### Options

* **-p producer**
    Sets producer id and name (default: 1201,ncprod). Format: id,name (e.g. "-p 123,MyProducer")
