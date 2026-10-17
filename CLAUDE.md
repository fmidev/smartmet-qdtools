# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`smartmet-qdtools` is a collection of ~46 command-line tools for converting, manipulating, and inspecting FMI's QueryData format — a native gridded/point weather data format used throughout the SmartMet ecosystem. Tools fall into two categories: **converters** (gribtoqd, bufrtoqd, nctoqd, h5toqd, etc.) and **querydata manipulators** (qdinfo, qdpoint, qdarea, qdcrop, qdcombine, etc.).

## Build commands

```bash
make -j all          # Build everything (debug/release/profile also work)
make qdinfo          # Build a single tool by name
make test            # Run full test suite
make format          # clang-format all source
make clean           # Clean build artifacts
make rpm             # Build RPM package
make install         # Install to $PREFIX/bin (default /usr/bin)
```

## Running a single test

```bash
cd test && make test-data && ./qdinfo.test    # Run one test file
cd test && make test                           # Run all tests via RunTests.sh
```

Tests are Perl scripts (`*.test`) that run tool commands and diff output against expected results in `results/`. Test data is symlinked from `/usr/share/smartmet/test/data/qdtools/`. The test framework module is `test/QDToolsTest.pm`. No tests are currently marked XFAIL in `RunTests.sh`.

## Build architecture

- `main/` — one `.cpp` per tool (entry point with `main()`)
- `source/` — shared implementation compiled into `obj/libqdtools.a`
- `include/` — headers for the shared library
- Each tool links against `libqdtools.a` plus system/SmartMet libraries
- Per-tool extra link dependencies are specified via target-specific `EXTRA_LIBS` in the Makefile (e.g., gribtoqd needs `-leccodes`, nctoqd needs `-lnetcdf_c++4 -lnetcdf`)
- `-fpermissive` is added to the compiler flags due to mdsplib headers
- jemalloc is linked first into every tool to reduce the allocation overhead of the eccodes BUFR/GRIB decoders

## Key dependencies

SmartMet libraries: `newbase` (QueryData format), `macgyver` (utilities), `smarttools` (scripting), `gis` (projections/GDAL), `calculator`, `imagine`. External: GDAL, NetCDF, HDF5, eccodes (GRIB and BUFR), the OPERA bufr library (radartoqd), mdsplib (metar2qd), jemalloc, Boost (program_options, regex, iostreams, thread), fmt 12.x.

## Code conventions

- Missing values use `kFloatMissing` constant from newbase
- SmartMet headers included as `<newbase/NFmiQueryData.h>`, `<macgyver/TimeZones.h>`, etc.
- Project headers included with quotes: `#include "QueryDataManager.h"`
- Newer tools use `boost::program_options` for CLI argument parsing, older ones newbase `NFmiCmdLine` (single-letter options, `!` marks an option with a value)
- `cnf/` contains runtime config files (netcdf.conf, grib.conf, bufr.conf, stations.csv, parameters.csv) installed to `/usr/share/smartmet/`
