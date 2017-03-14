// GeoTiff.h

#ifndef GeoTiffQD_H
#define GeoTiffQD_H

#include <string>
#include <cstdio>

#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiRotatedLatLonArea.h>

#include <boost/optional.hpp>

using namespace std;

#define BOTTOMLEFT 0
#define TOPRIGHT 1
#define NCORNERS (TOPRIGHT + 1)

enum GeomDefinedType
{
  kUndefinedGeom,
  kLatLonGeom,
  kRotatedGeom,
  kStereoGeom,
  kStereoGeom10,
  kStereoGeom20,
  kYkjGeom
};

class GeoTiffQD
{
 public:
  virtual ~GeoTiffQD(void) {}
  GeoTiffQD(int code = 0);

  GeomDefinedType ConverQD2GeoTiff(string aNameVersion,
                                   NFmiFastQueryInfo *theData,
                                   NFmiFastQueryInfo *theExternal,
                                   string tsrs,
                                   bool isIntDataType,
                                   double scale);
  void DestinationProjection(NFmiArea *destProjection);
  void SetTestMode(bool testMode);

 private:
  void getregllbbox(const NFmiRotatedLatLonArea *a);
  NFmiFastQueryInfo *generateLatLonInfo(NFmiFastQueryInfo *orginData);
  int *fillIntRasterByQD(NFmiFastQueryInfo *theData,
                         NFmiFastQueryInfo *theSecondData,
                         int width,
                         int height,
                         const NFmiArea *area);
  float *fillFloatRasterByQD(NFmiFastQueryInfo *theData,
                             NFmiFastQueryInfo *theSecondData,
                             int width,
                             int height,
                             const NFmiArea *area);
  double calculateTrueNorthAzimuthValue(float value, const NFmiArea *area, const NFmiPoint *point);

  void ConvertToGeoTiff(string aNameVersion,
                        NFmiFastQueryInfo *theData,
                        NFmiFastQueryInfo *theExternal,
                        GeomDefinedType geomDefinedType);

 private:
  int itsCode;
  bool isIntDataType;
  double itsScale;
  bool isDrawGridLines;

 public:
  NFmiArea *itsDestProjection;
};

inline GeoTiffQD::GeoTiffQD(const int code) : itsCode(code) {}
inline void GeoTiffQD::DestinationProjection(NFmiArea *destProjection)
{
  itsDestProjection = destProjection;
}

#endif  // GeoTiffQD_H
