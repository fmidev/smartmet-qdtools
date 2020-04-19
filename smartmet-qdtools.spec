%define BINNAME qdtools
%define RPMNAME smartmet-%{BINNAME}
Summary: Command line tools for handling querydata
Name: %{RPMNAME}
Version: 20.4.18
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Tools
URL: https://github.com/fmidev/smartmet-qdtools
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: boost169-devel
BuildRequires: bzip2-devel
BuildRequires: eccodes-devel
BuildRequires: eccodes
BuildRequires: hdf5-devel >= 1.8.12
BuildRequires: libbufr >= 3.2
BuildRequires: libecbufr
BuildRequires: libjpeg-devel
BuildRequires: libpng-devel
BuildRequires: fmt-devel
BuildRequires: smartmet-library-calculator-devel >= 20.4.18
BuildRequires: smartmet-library-gis-devel >= 20.4.18
BuildRequires: smartmet-library-imagine-devel >= 20.4.18
BuildRequires: smartmet-library-macgyver-devel >= 20.4.18
BuildRequires: smartmet-library-newbase-devel >= 20.4.18
BuildRequires: smartmet-library-smarttools-devel >= 20.4.18
BuildRequires: smartmet-timezones
BuildRequires: smartmet-library-spine-devel
BuildRequires: mdsplib >= 16.4.8
BuildRequires: netcdf-devel >= 4.3.3.1
BuildRequires: zlib-devel
BuildRequires: gdal30-devel
BuildRequires: jasper-devel
BuildRequires: mxadatamodel
BuildRequires: netcdf-cxx-devel
BuildRequires: fmt-devel
Requires: smartmet-timezones >= 19.7.29
Requires: smartmet-library-calculator >= 20.4.18
Requires: smartmet-library-gis >= 20.4.18
Requires: smartmet-library-imagine >= 20.4.18
Requires: smartmet-library-macgyver >= 20.4.18
Requires: smartmet-library-newbase >= 20.4.18
Requires: smartmet-library-smarttools >= 20.4.18
Requires: smartmet-library-spine
Requires: fmt
Requires: eccodes
Requires: fmt
Requires: hdf5 >= 1.8.12
Requires: jasper-libs >= 1.900.1
Requires: libecbufr
Requires: libbufr >= 3.2
Requires: netcdf >= 4.3.3.1
Requires: gdal30-libs
Requires: boost169-regex
Requires: boost169-iostreams
Requires: boost169-date-time
Requires: boost169-program-options
Requires: boost169-thread
Requires: boost169-filesystem
Requires: boost169-system
Requires: bzip2-libs
Requires: glibc
Requires: libgcc
Requires: libjpeg
Requires: libpng
Requires: libstdc++
Requires: zlib
Provides: ashtoqd = %{Version} 
Provides: bufrtoqd = %{Version} 
Provides: combinepgms2qd = %{Version} 
Provides: combineHistory = %{Version}
Provides: csv2qd = %{Version} 
Provides: flash2qd = %{Version} 
Provides: grib2tojpg = %{Version} 
Provides: gribtoqd = %{Version} 
Provides: grib2toqd = %{Version} 
Provides: h5toqd = %{Version} 
Provides: kriging2qd = %{Version}
Provides: laps2qd = %{Version} 
Provides: metar2qd = %{Version} 
Provides: nctoqd = %{Version} 
Provides: nc2qd = %{Version} 
Provides: radartoqd = %{Version} 
Provides: pgm2qd = %{Version} 
Provides: qd2csv = %{Version} 
Provides: qdtogrib = %{Version} 
Provides: qdversionchange = %{Version} 
Provides: synop2qd = %{Version} 
Provides: temp2qd = %{Version} 
Provides: wrftoqd = %{Version} 
Provides: qdarea = %{Version} 
Provides: qdcheck = %{Version} 
Provides: qdcombine = %{Version} 
Provides: qdcrop = %{Version} 
Provides: qddiff = %{Version} 
Provides: qddifference = %{Version} 
Provides: qdextract = %{Version} 
Provides: qdfilter = %{Version} 
Provides: qdgridcalc = %{Version} 
Provides: qdinfo = %{Version} 
Provides: qdinterpolatearea = %{Version} 
Provides: qdinterpolatetime = %{Version} 
Provides: qdmissing = %{Version} 
Provides: qdpoint = %{Version} 
Provides: qdproject = %{Version} 
Provides: qdscript = %{Version} 
Provides: qdset = %{Version} 
Provides: qdsmoother = %{Version} 
Provides: qdsounding = %{Version} 
Provides: qdsoundingindex = %{Version} 
Provides: qdsplit = %{Version} 
Provides: qdstat = %{Version} 
Provides: qdview = %{Version} 
Obsoletes: smartmet-qdconversion < 17.1.10
Obsoletes: smartmet-qdconversion-debuginfo < 17.1.10
Obsoletes: smartmet-qdtools < 17.1.10
Obsoletes: smartmet-qdtools.debuginfo < 17.1.10
Obsoletes: smartmet-qdarea < 17.1.12
Obsoletes: smartmet-qdarea-debuginfo < 17.1.12

%description
Command line tools for handling querydata

%prep
%setup -q -n %{RPMNAME}
 
%build
make %{_smp_mflags}

%install
%makeinstall

%clean

%files
%defattr(0775,root,root,0775)
%{_bindir}/ashtoqd
%{_bindir}/bufrtoqd
%{_bindir}/combinepgms2qd
%{_bindir}/combineHistory
%{_bindir}/csv2qd
%{_bindir}/flash2qd 
%{_bindir}/grib2tojpg
%{_bindir}/grib2toqd
%{_bindir}/gribtoqd
%{_bindir}/h5toqd
%{_bindir}/kriging2qd
%{_bindir}/laps2qd
%{_bindir}/metar2qd
%{_bindir}/nc2qd
%{_bindir}/nctoqd
%{_bindir}/pgm2qd
%{_bindir}/qd2csv 
%{_bindir}/qd2geotiff 
%{_bindir}/qdarea
%{_bindir}/qdcheck
%{_bindir}/qdcombine
%{_bindir}/qdcrop 
%{_bindir}/qddiff 
%{_bindir}/qddifference
%{_bindir}/qdextract
%{_bindir}/qdfilter 
%{_bindir}/qdgridcalc
%{_bindir}/qdinfo 
%{_bindir}/qdinterpolatearea
%{_bindir}/qdinterpolatetime
%{_bindir}/qdmissing 
%{_bindir}/qdpoint 
%{_bindir}/qdproject 
%{_bindir}/qdscript
%{_bindir}/qdset
%{_bindir}/qdsmoother 
%{_bindir}/qdsounding 
%{_bindir}/qdsoundingindex
%{_bindir}/qdsplit 
%{_bindir}/qdstat
%{_bindir}/qdtogrib
%{_bindir}/qdversionchange
%{_bindir}/qdview
%{_bindir}/radartoqd
%{_bindir}/synop2qd 
%{_bindir}/temp2qd 
%{_bindir}/wrftoqd
%defattr(0664,root,root,0775)
%{_datadir}/smartmet/dictionaries/*.conf
%{_datadir}/smartmet/formats/grib.conf
%{_datadir}/smartmet/formats/netcdf.conf
%{_datadir}/smartmet/formats/bufr.conf
%{_datadir}/smartmet/parameters.csv
%{_datadir}/smartmet/stations.csv

%changelog
* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgrade to Boost 1.69

* Thu Apr  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.2-1.fmi
- Added combineHistory

* Wed Apr  1 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.1-1.fmi
- Use NFmiCoordinateMatrix instead of NFmiDataMatrix<NFmiPoint>

* Fri Mar 27 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.3.27-1.fmi
- Added kriging2qd

* Fri Feb 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.21-1.fmi
- qdstat now skips WindVectorMS automatically for not being suitable for simple mean/max calculations

* Wed Feb 19 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.19-1.fmi
- Fixed nctoqd -h output

* Tue Feb 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.18-2.fmi
- Fixed bufrtoqd data copying loop to use --usebufrname correctly

* Tue Feb 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.18-1.fmi
- metar2qd option -W prevents packaging wind components to one TotalWind parameter

* Thu Feb 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.13-1.fmi
- bufrtoqd now uses config file column 1 (code) instead of column 2 (name) to avoid ambiguities for parameters with the same name
- Added bufrtoqd --usebufrname to revert to the old behaviour

* Thu Feb  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.6-1.fmi
- NFmiPoint Z-coordinate was removed from the ABI

* Fri Jan 17 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.17-1.fmi
- qdcombine can now combine point data too

* Wed Jan  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.8-1.fmi
- NFmiPoint ABI changed

* Fri Dec 13 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.13-1.fmi
- Repackaged due to NFmiArea API changes

* Wed Dec 11 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.11-1.fmi
- Upgrade to GDAL 3.0

* Wed Dec  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.4-1.fmi
- Fixed dependency to be on gdal-libs instead of gdal
- Use -fno-omit-frame-pointer for a better profiling and debugging experience                                                                                              
* Fri Nov 29 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.29-1.fmi
- Increased space reserved for qdstat counters

* Fri Nov 22 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.22-1.fmi
- qdinfo now displays parameter limits with "-" instead of 32700

* Thu Nov 21 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.21-1.fmi
- gribtoqd now supports shape of the earth parameters

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.fmi
- Repackaged due to newbase API changes

* Tue Nov 19 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.19-1.fmi
- Repackaged to get NFmiStreamQueryData default info version up to 7

* Tue Nov 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.12-2.fmi
- Fixed nctoqd to work for sample polar stereographic data

* Tue Nov 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.12-1.fmi
- Added nctoqd --info option for printing dimensionality summary
- Added nctoqd --xdim --ydim --zdim and --tdim options
- Added level handling to nctoqd
- Fixed nctoqd to analyze dimensions from NcDim variables instead of variable units

* Tue Nov  5 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.5-1.fmi
- gribtoqd no longer limits output size, option -m has no effect

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuilt due to newbase API/ABI changes

* Tue Oct 22 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.22-1.fmi
- Added PressureTendency to default parameters.csv file

* Fri Sep 27 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.27-1.fmi
- Repackaged due to ABI changes in SmartMet libraries

* Thu Sep 19 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.9.19-1.fmi
- gribtoqd uses separate parameter structure to store pressure parameter info for ground data RH calculation

* Wed Sep 11 2019 Pertti Kinnia <pertti.kinnia@fmi.fi> - 19.9.11-1.fmi
- gribtoqd accepts ground level data to calculate RH

* Thu Sep  5 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.5-1.fmi
- bufrtoqd now tries to choose the station data with the most "accurate" coordinates

* Wed Sep  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.4-1.fmi
- Added bufrtoqd --roundtohours option
- bufrtoqd now uses stations.csv coordinates instead of message coordinates to avoid multiple instances of the same station

* Tue Sep  3 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.3-1.fmi
- Fixed qdinfo to recognize LCC and WebMercator projections

* Tue Jun 11 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.11-1.fmi
- Added ILHF to the stations list

* Fri Jun  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.7-1.fmi
- gribtoqd now supports the Lambert conformal projection

* Mon Apr  1 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.1-1.fmi
- Fixed gribtoqd to handle projection types in version independent manner

* Thu Mar  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.3.7-1.fmi
- Added qdpoint --wgs84 option

* Wed Feb 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.20-1.fmi
- Added Visibility to parameters.csv

* Fri Feb 15 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.15-1.fmi
- h5toqd now has default filename replacements for "%PLC" etc, and the defaults can be modified from the command line
- option -h now documents the available filename replacements

* Thu Feb  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.7-1.fmi
- Improved bufrtoqd error checking on bad message dates

* Tue Nov 13 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.11.13-1.fmi
- Adjust invalid NFmiMetTime timestamps (stepped forward to nonexistent date, e.g. 31.9) to 1'st day of next month

* Sun Sep 16 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.16-1.fmi
- Repackaged since calculator library API changed

* Wed Aug 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.22-1.fmi
- Fixed typo in bufrtoqd help text

* Tue Aug 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.14-1.fmi
- Fixed bufr replication flag to be persistent across messages in a single file

* Wed Jun 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.13-1.fmi
- Added option h5toqd --startepochs to use the interval start time as the valid time

* Wed May 16 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.16-1.fmi
- Added option qdcrop -n for renaming parameters

* Sat May 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.12-1.fmi
- gribtoqd -y is now deprecated and j-coordinate scan direction is detected automatically

* Fri May  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.4-2.fmi
- h5toqd now works for RaVaKe data which may store scalars into one element vectors

* Fri May  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.4-1.fmi
- For now gribtoqd and gribt2toqd error on flipped data only for stereographic projections

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-3.fmi
- h5toqd case options now have an effect on output filename only, not on producername

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-2.fmi
- h5toqd producername is now expanded similarly to the output filename

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-1.fmi
- h5toqd now allows PCAPPI + RATE combination, output will be PrecipitationRate
- h5toqd now recognizes %INTERVAL as a valid output filename pattern

* Wed May  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.2-1.fmi
- Repackaged since newbase NFmiEnumConverter ABI changed

* Fri Apr 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.27-1.fmi
- Fixed h5toqd to generate a compact time descriptor
- Added option --prodparfix to h5toqd
- Added possibility to format output filename based on data contents

* Wed Apr 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.25-1.fmi
- Fixed metar2qd to handle fog correctly
- gribtoqd and grib2toqd now error if j-scan direction is negative, required projection calculations are not implemented

* Tue Apr 24 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.24-1.fmi
- qdtogrib -p or --packing can now be used to set the packing method

* Wed Apr 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.18-1.fmi
- qdcrop -R enables reading a directory of files to be combined

* Wed Apr 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.11-1.fmi
- Removed obsolete Oslo Fornebu from the stations list

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Upgrade to boost 1.66

* Wed Apr  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.4-1.fmi
- metar2qd now handles month changes better for non-NOAA type METARs

* Tue Mar 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.27-1.fmi
- qdpoint -h now lists the available meta parameters

* Thu Mar 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.22-2.fmi
- Changed MetaNorth calculation to use NFmiArea::TrueNorthAzimuth

* Thu Mar 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.22-1.fmi
- qdpoint: Added MetaNorth parameter

* Fri Mar 16 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.16-1.fmi
- csv2qd: added option --allstations (-A) to generate output with all listed stations

* Thu Mar 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.15-1.fmi
- qdcheck: warnings is now in caps for easier detection
- qdcheck: fixed some messages to be in English

* Thu Mar  8 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.8-1.fmi
- qdstat now recognizes precipitation form values for snow grains and ice pellets

* Wed Mar  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.7-3.fmi
- qdtogrib no longer has a default producer, the default tables will be used instead

* Wed Mar  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.7-2.fmi
- Fixed RPM requirements

* Wed Mar  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.7-1.fmi
- qdinfo: improved tabular layout for parameter info
- qdcheck: memory map input data for improved speed

* Tue Mar  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.6-1.fmi
- qdinfo: -p and -P options now report the allowed parameter range

* Mon Mar  5 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.5-2.fmi
- qdtogrib: fixed rotated latlon coordinates to work

* Mon Mar  5 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.5-1.fmi
- nctoqd: Added tolerance option
- nctoqd: Fix NetCDF version comparison for exactly matching conventions
- metar2qd: use current time as header if metar data is not in NOAA format

* Thu Feb 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.15-1.fmi
- qdtogrib -I (--ignore-origintime) sets forecast time to first valid time instead of origin time

* Mon Feb 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.12-1.fmi
- Added CF checking options to nctoqd

* Thu Feb  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.1-1.fmi
- Added qdstat text printout for SmartSymbol parameter

* Wed Jan 31 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.31-1.fmi
- nctoqd improvements
- gribtoqd now accepts levels of type 'atmosphere'

* Thu Jan 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.25-1.fmi
- qdtogrib -C/--centre sets the centre name or number
- qdtogrib -S/--subcentre sets the subcentre number
- qdtogrib -L/--list-centres list the known centre names and numbers
- Fixed qdtogrib generation of dx/dy values for polar stereographic data
- C++ modernization in several files

* Mon Jan 22 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.22-1.fmi
- qdset -w changes station id to a new one
- qdset -W changes station name to a new one

* Fri Jan 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.19-1.fmi
- qdfilter -Q will process all querydata in the given directory as a multifile

* Thu Jan 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.18-1.fmi
- csv2qd now only warns if it encounters an unknown station
- added option csv2qd --quiet

* Mon Jan 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.15-1.fmi
- Added options -Z (--allevels) and -z (--levels) to qdstat

* Fri Jan 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.12-2.fmi
- nctoqd improvements

* Fri Jan 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.12-1.fmi
- Fixed qdtogrib to handle times before origin time correctly
- Fixed qdtogrib to be able to handle minute level data

* Wed Dec 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.20-1.fmi
- nctoqd improvements

* Tue Dec 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.19-2.fmi
- qddifference now detects different grids in the input files

* Tue Dec 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.19-1.fmi
- Fixed handling of the last bin in qdstat

* Sat Dec 16 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.16-1.fmi
- Added handling of PotentialPrecipitationType and PotentialPrecipitationForm
- Added sandstorm as a potential value for FogIntensity
- Added automatic scaling for binning

* Thu Dec  7 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.7-1.fmi
- qdtogrib now produces a missing value bitmap if the data contains missing values

* Thu Nov 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.23-1.fmi
- nctoqd is now able to handle more model data

* Wed Nov  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.1-1.fmi
- Rubuilt due to GIS-library API change

* Thu Oct 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.19-1.fmi
- bufrtoqd now fcloses the files to avoid reaching limits on the number of open files

* Wed Oct 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.18-1.fmi
- bufrtoqd now skips bad messages in individual files instead of skipping the rest of the file

* Fri Oct 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.13-1.fmi
- bufrtoqd now ignores insignificant sounding levels by default
- Replaced bufrtoqd -S and --significance by --insignificant

* Wed Oct 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.11-2.fmi
- bufrtoqd no longer crashes if opening a file for reading fails

* Wed Oct 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.11-1.fmi
- bufrtoqd option -S: skip sounding levels which have been marked as insignificant (code 8042)
- bufrtoqd now validates the argument to option -C (--category)

* Mon Oct  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.9-2.fmi
- gribtoqd can now take multiple files, directories or patterns as parameters
- gribtoqd will no longer crash if a level value is missing, the message will be ignored

* Mon Oct  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.9-1.fmi
- qdfilter -i and -I options now accept multiple hour selections simultaneously

* Wed Sep 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.27-1.fmi
- qdversionchange option -N will convert TotalCloudiness from octas to percents

* Thu Sep 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.14-1.fmi
- Switched from grib_api to eccodes

* Wed Sep 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.13-3.fmi
- qdarea not uses the input shape even if not fully inside the single specified querydata
- Added qdarea -Q option to silence warnings on above

* Wed Sep 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.13-2.fmi
- nctoqd now handles attribute_fill value correctly

* Wed Sep 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.13-1.fmi
- qdstat now fails more gracefully if the statistics overflow column widths
- qdstat now prints parameter number if the name is unknown

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65

* Tue Aug 22 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.22-2.fmi
- Fixed qdarea -c to work

* Tue Aug 22 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.22-1.fmi
- qdarea::querydata is no longer required if qdarea -q is used

* Mon Aug 21 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.21-2.fmi
- qdinfo -x now prints the PROJ.4 string for the data

* Mon Aug 21 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.21-1.fmi
- qdinfo -x now prints the data bounding box in native world coordinates

* Fri Aug 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.18-1.fmi
- qdset: added option -T for setting a new origin time in UTC
- gribtoqd: fixed to work with WAM data for which 1st and last longitudes are zero

* Tue Jun 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.6.20-1.fmi
- nctoqd: improved axis name handling

* Thu Jun 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.6.15-1.fmi
- Fixed dependency to be on jasper-libs and not on jasper

* Mon Jun  5 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.6.5-1.fmi
- Added qdcrop -m parameter,limit

* Mon May 29 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.29-1.fmi
- Fixed qdstat to count the maximum value into distributions only once

* Tue May  9 2017 Mikko Visa <mikko.visa@fmi.fi> - 17.5.9-1.fmi
- Fix qd2geotiff projection bug

* Fri May  5 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.5-1.fmi
- Added synop2qd -r option for setting a reference time different from the wall clock for testing purposes

* Wed May  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.3-2.fmi
- qdtogrib now handles negative lead times by ignoring them
- qdtogrib -v now reports if some time steps are omitted for having negative lead times
- qdtogrib -D or --dump now dumps GRIB contents using grib_api, used to be done with -v

* Wed May  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.3-1.fmi
- Moved qdsignificantlevelfilter and qdclimatologydatasmoother to fmitools-package

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-1.fmi
- Switched to using mdsplib from GitHub

* Tue Apr  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.4-1.fmi
- Fixed qdversionchange to use fog and probability of thunder parameters properly

* Mon Apr  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.3-1.fmi
- Added qdclimatologydatasmoother
- qdversionchange option -F specifies fog parameter to be used
- qdversionchange option -P specifies probability of thunder parameter to be used
- qdversionchange now uses all cores by default for extra speed, use -m to override

* Wed Mar 22 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.22-1.fmi
- h5toqd now works with more parameters
- h5toqd now allows gain, offset and undetect to be missing

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-2.fmi
- Recompiled with the latest macgyver

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-1.fmi
- Added qdsignificantlevelfilter

* Thu Mar  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.9-1.fmi
- Updated to use the latest newbase and smarttools

* Fri Feb 24 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.24-1.fmi
- Fixed qdtogrib scanning modes, the output was upside down

* Mon Feb 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.13-1.fmi
- Recompiled due to newbase API change

* Mon Feb  6 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.6-1.fmi
- nctoqd now recognizes more ways to identify axis variables
- Added --mmap option to nctoqd to enable memory mapping the output file

* Thu Feb  2 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.2-2.fmi
- Added qdsplit option -T which memory maps output
- Added qdcombine option -O which memory maps output

* Thu Feb  2 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.2-1.fmi
- Fixed qdsplit to loop over the number of times instead of parameters

* Mon Jan 30 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.30-1.fmi
- qdsplit option t can now be used to speed up the program for large input files
- qdsplit now ignores calculating the missing percentage if the limit is >= 100.

* Fri Jan 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.27-1.fmi
- Recompiled due to NFmiQueryData object size change

* Mon Jan 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.23-1.fmi
- Optimized qdcombine for speed

* Wed Jan 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.18-1.fmi
- Added PCAPPI level support to h5toqd

* Thu Jan 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.12-2.fmi
- Added qddifference since it is needed in the tests

* Thu Jan 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.12-1.fmi
- Added qdarea from deprecated textgenapps package

* Tue Jan 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.10-1.fmi
- Updated to use FMI open source naming conventions

* Fri Dec 30 2016 Veikko Punkka <veikko.punkka@fmi.fi> - 16.12.30-1.fmi
- Merged qdconversion into qdtools

* Fri Sep 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.30-1.fmi
- Recompiled - Latvia seems to have a stange RHEL6 version

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Recompiled due to newbase API changes

* Fri May 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.20-2.fmi
- Fixed qdscript to add the parameter only once

* Fri May 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.20-1.fmi
- Fixed qdscript to preserve the producer

* Tue Apr 19 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.4.19-1.fmi
- Fixed qdcrop option -W
- Improved qdcrop option -S documentation

* Thu Mar 17 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.3.17-1.fmi
- New newbase parameters

* Tue Jan 26 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.26-1.fmi
- qdcrop -M now picks the desired minute from the data

* Sun Jan 17 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.17-1.fmi
- newbase API changed

* Mon Nov 30 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.30-1.fmi
- qdstat distributions now describe enumerated roadmodel parameter values

* Mon Nov 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.23-1.fmi
- Optimized loops in qdcheck, qdcombine, qdcrop, qdfilter, qdmissing, qdsplit and qdstat

* Mon Nov  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.9-1.fmi
- Recompiled due to new newbase parameter names

* Wed Oct 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.28-1.fmi
- Stopped using deprecated number_cast functions

* Mon Jun 22 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.22-1.fmi
- Added MetaFeelsLike
- Added MetaSummerSimmer

* Wed Jun 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.17-1.fmi
- Recompiled to recognize new de-icing parameters

* Wed May 27 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.5.27-1.fmi
- Added options -i and -o to qdinterpolatetime

* Wed Apr 15 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.15-1.fmi
- newbase NFmiQueryData::LatLonCache API changed

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Mon Mar 30 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.3.30-1.fmi
- Link newbase etc dynamically

* Mon Mar 16 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.3.16-1.fmi
- qdsounding no longer requires the locations file if only coordinate arguments are used

* Mon Mar  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.3.9-1.fmi
- Added qdcrop option -A for keeping analysis time parameters

* Tue Feb 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.17-2.fmi
- Recompiled to get parameter PrecipitationInstantTotal into use
- Fixed qdcrop -d option to reduce the grid size when necessary

* Tue Feb 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.17-1.fmi
- Rebuilt with fixes to nearest neighbour interpolation for global data

* Mon Feb 16 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.16-1.fmi
- Rebuilt with more interpolation fixes to newbase

* Wed Feb 11 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.11-1.fmi
- Rebuilt with interpolation fixes to newbase

* Fri Dec  5 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.5-1.fmi
- Rebuilt with new ravake parameters

* Tue Nov 11 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.11-1.fmi
- Rebuilt with new PrecipitationForm3 parameter

* Mon Oct 13 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.10.13-1.fmi
- Rebuilt with new precipitation and buoyancy parameters (EC data)

* Wed Oct  1 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.1-1.fmi
- Rebuilt with the new SYKE parameter

* Fri Sep 26 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.26-1.fmi
- qdsounding now uses qdpoint::coordinates setting
- qdsounding default coordinates path is now /smartmet/share/coordinates/default.txt

* Tue Sep 16 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.16-1.fmi
- qdpoint::timezone is now by default 'local'

* Fri Sep 12 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.12-1.fmi
- qdset -Z enables changing the level value of the data (assumed to be single level)
- qdset -L enables changing the level type of the data

* Thu Sep 11 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.11-1.fmi
- qdpoint now has a default coordinate file /smartmet/share/coordinates/default.txt
- qdpoint now uses qdpoint::coordinates setting like other commands
- qdpoint::coordinates_path and qdpoint::coordinates_file are not used

* Fri Aug  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.8-1.fmi
- Added a default dictionary for qdscripta

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-2.fmi
- qdinfo now reports the correct WKT for latlon and rotated latlon projections

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-1.fmi
- qdinfo now reports the correct WKT for latlon and rotated latlon projections

* Fri Aug  1 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.1-1.fmi
- Added qdextract
- Recompiled to recognize latest newbase parameters

* Thu Oct 17 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.17-1.fmi
- Fixed qdstat to print nan min, mean and max when there are no valid data values
- Added option -Q to qdpoint for reading an entire directory at once
- Speed optimizations to qdsoundingindex

* Thu Oct 10 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.10-2.fmi
- Fixed qdstat to display the first bin too

* Thu Oct 10 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.10-1.fmi
- Added qdstat command

* Wed Oct  9 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.9-1.fmi
- qdview -S works like -s, but draws only points with valid data
- Added option -p for selecting the parameter for which the valid data is checked

* Thu Oct  3 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.10.3-1.fmi
- Fixed qdscript to produce binary output on Windows
- Fixed a duplicate initialization of settings in qdscript
- Added safety checks against null results in qdscript

* Thu Sep 12 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.9.12-1
- Extended qdcrop option -w to cover ranges of stations to keep and added option -W for removing stations

* Tue Sep 10 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.9.10-1.fmi
- Recompiled against the new Newbase

* Thu Sep  5 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.9.5-1.fmi
- Added option -o to qdfilter and qdcombine

* Tue Aug 13 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.13-1.fmi
- Added dependency on smartmet-timezones

* Mon Aug  5 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.5-1.fmi
- qdinterpolatearea now supports options -i and -o for input/output files

* Fri Aug  2 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.2-1.fmi
- Recompiled with library fixes for handling Pacific querydatas

* Thu Aug  1 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.8.1-1.fmi
- Fixed qdsplit to extract the basename properly if input file is a directory

* Wed Jul 31 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.31-1.fmi
- Optimized qdsplit, qddiff and qdsmoother to use memory mapped files. Also, "-" can be used as output filename for querydata

* Tue Jul 23 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.23-1.fmi
- Fixed thread safety issues which could have effected programs using multiple cores

* Wed Jul  3 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54

* Tue Jul  2 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.2-1.fmi
- qdfilter now supports the change function (last value minus the first one)

* Tue Jun 25 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.25-1.fmi
- Recompiled to get 19 new parameter names into use

* Mon May 27 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.5.27-1.fmi
- Recompiled to get new parameter names into use

* Wed Apr 17 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.17-1.fmi
- qdset now uses memory mapping techniques for speed

* Mon Apr  8 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.8-1.fmi
- qdviwe now uses memory mapping to speed up execution

* Thu Mar 21 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.21-2.fmi
- Added option -Z to qdmissing to disable printing unimportant information
- Added option -w to qdmissing to analyze which stations have missing data

* Thu Mar 21 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.21-1.fmi
- newbase has new parameter names for gust U- and V-components

* Wed Mar  6 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.6-3.fmi
- qdinfo -P now prints parameter interpolation method and precision

* Wed Mar  6 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.6-2.fmi
- Bug fix release, newbase interpolation was bugged

* Wed Mar  6 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.3.6-1.fmi
- Fixed qdmissing to work for > 2GB files

* Fri Feb 22 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.2.22-1.fmi
- Added option -m to qdcrop for excluding timesteps with too much missing data

* Mon Jan 14 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.1.14-1.fmi
- Recompiled to get new parameter FlagDate into use

* Thu Jan 10 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.1.10-1.fmi
- qdinfo now prints the center latlon too
- qdinfo now prints the WKT only in Unix

* Fri Jan  4 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.1.4-1.fmi
- Recompiled due to new newbase parameter names

* Fri Dec 28 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.12.28-1.fmi
- Added option -q to qdproject

* Wed Nov 28 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.28-1.fmi
- Added option --check-dewpoint-difference to qdcheck

* Tue Nov 27 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.27-1.el6.fmi
- Bug fix release (newbase + smarttools libraries)

* Tue Oct  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.10.2-1.el6.fmi
- New parameter names for ice storage from newbase

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi
- Migration to boost 1.50

* Wed Jul  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.4-1.el6.fmi
- New fractile parameters in newbase

* Wed Apr 18 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.18-1.el6.fmi
- qdinfo -h now prints correct default value for option -T
- qdinfo now recognizes depth data when printing level information
- qdpoint -w will now skip stations with invalid coordinates (32700,32700)

* Thu Mar 29 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.29-1.el6.fmi
- qdcombine now uses memory mapping to be more efficient
- qdcombine now loops over data differently to save half of memory use

* Mon Mar 19 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.19-1.el6.fmi
- New parameter names for LAPS in newbase

* Mon Mar 12 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.12-1.el6.fmi
- Added option -a to qdscript for adding parameters to the data

* Fri Mar  9 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.9-1.el6.fmi
- qdcrop -a no longer stops if a parameter already exists in the data, it is simply kept

* Fri Feb 24 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.24-1.el5.fmi
- Recompiled with the latest newbase

* Mon Feb 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.13-2.el5.fmi
- Added qdsounding -x and -y options

* Mon Feb 13 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.13-1.el5.fmi
- qdsounding options -p and -P swapped meanings

* Wed Feb  8 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.8-1.el6.fmi
- New parameternames into use: IceSpeed and IceDirection

* Tue Feb  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.2.7-1.el5.fmi
- Recompiled with latest newbase parameternames and fixed wind chill formula

* Thu Jan 12 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.1.12-1.el6.fmi
- Changed qdfilter to use textgen integration methods

* Fri Dec 16 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.16-1.el6.fmi
- macgyver library bug fix

* Wed Dec 14 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.14-1.el6.fmi
- qdgridcalc still had a couple words in Finnish

* Tue Dec 13 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.13-1.el6.fmi
- qdcrop now checks the sanity of the time options to prevent segmentation faults

* Thu Nov 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.24-1.el6.fmi
- qdinfo etc now recognize RadarBorder parameter

* Fri Nov 11 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.11-1.el6.fmi
- qdpoint now uses a shapepack to deduce timezones

* Fri Nov  4 2011 tervo <roope.tervo@fmi.fi> - 11.11.4-1.el6.fmi
- Fixed local timezone area to be from north to Gibraltar

* Thu Nov  3 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.11.3-1.el6.fmi
- Fixed qdset error message to be in English

* Fri Oct 28 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.28-1.el6.fmi
- qdpoint -P now accepts numerical parameter names
- qdpoint -v now uses English only

* Tue Oct 25 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.25-1.el6.fmi
- Added qdview from the qdmisc rpm

* Thu Oct 20 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.20-1.el5.fmi
- qdinfo now reports level name as well

* Mon Oct 17 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.17-1.el5.fmi
- Latest newbase has a new AshConcentration parameter

* Wed Oct  5 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.10.5-1.el5.fmi
- Upgraded to latest newbase with new parameter names

* Tue Aug 23 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.8.23-1.el5.fmi
- qdinfo no longer segfaults if -q is not used properly

* Wed Jul 20 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.7.20-1.el5.fmi
- Upgrade to boost 1.47

* Thu Jul  7 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.7.7-1.el5.fmi
- Recompiled to get new parameter names into use

* Tue May 31 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.5.31-1.el5.fmi
- Recompile forced by major update to base libraries

* Thu May 19 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.5.19-1.el5.fmi
- Merged tools from qdutils and qdputket
