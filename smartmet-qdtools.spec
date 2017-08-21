%define BINNAME qdtools
%define RPMNAME smartmet-%{BINNAME}
Summary: Command line tools for handling querydata
Name: %{RPMNAME}
Version: 17.8.21
Release: 1%{?dist}.fmi
License: MIT
Group: Development/Tools
URL: https://github.com/fmidev/smartmet-qdtools
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: boost-devel
BuildRequires: bzip2-devel
BuildRequires: grib_api-devel >= 1.14.0
BuildRequires: hdf5-devel >= 1.8.12
BuildRequires: libbufr >= 3.2
BuildRequires: libecbufr
BuildRequires: libjpeg-devel
BuildRequires: libpng-devel
BuildRequires: smartmet-library-calculator-devel >= 17.3.16
BuildRequires: smartmet-library-gis-devel >= 17.3.14
BuildRequires: smartmet-library-imagine-devel >= 17.3.14
BuildRequires: smartmet-library-macgyver-devel >= 17.7.29
BuildRequires: smartmet-library-newbase-devel >= 17.8.1
BuildRequires: smartmet-library-smarttools-devel >= 17.6.13
BuildRequires: mdsplib >= 16.4.8
BuildRequires: netcdf-devel >= 4.3.3.1
BuildRequires: zlib-devel
BuildRequires: gdal-devel >= 1.11.4
BuildRequires: jasper-devel
BuildRequires: mxadatamodel
Requires: smartmet-timezones >= 17.4.12
Requires: smartmet-library-calculator >= 17.3.16
Requires: smartmet-library-gis >= 17.3.14
Requires: smartmet-library-imagine >= 17.3.14
Requires: smartmet-library-macgyver >= 17.7.29
Requires: smartmet-library-newbase >= 17.8.1
Requires: smartmet-library-smarttools >= 17.6.13
Requires: grib_api >= 1.14.0
Requires: hdf5 >= 1.8.12
Requires: jasper-libs >= 1.900.1
Requires: libecbufr
Requires: libbufr >= 3.2
Requires: netcdf >= 4.3.3.1
Requires: gdal >= 1.11.4
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
Provides: csv2qd = %{Version} 
Provides: flash2qd = %{Version} 
Provides: grib2tojpg = %{Version} 
Provides: gribtoqd = %{Version} 
Provides: grib2toqd = %{Version} 
Provides: h5toqd = %{Version} 
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

%check
make test

%clean

%files
%defattr(0775,root,root,0775)
%{_bindir}/ashtoqd
%{_bindir}/bufrtoqd
%{_bindir}/combinepgms2qd
%{_bindir}/csv2qd
%{_bindir}/flash2qd 
%{_bindir}/grib2tojpg
%{_bindir}/grib2toqd
%{_bindir}/gribtoqd
%{_bindir}/h5toqd
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
