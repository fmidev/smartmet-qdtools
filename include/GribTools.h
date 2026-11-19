// ======================================================================
/*!
 * Grib tools
 */
// ----------------------------------------------------------------------

#ifndef GRIBTOOLS_H
#define GRIBTOOLS_H

#include <newbase/NFmiLevel.h>
#include <newbase/NFmiParam.h>

#include <grib_api.h>
#include <string>
#include <vector>

// Debugging tools

void DUMP(grib_handle *grib);
void DUMP(grib_handle *grib, const char *ns);

// Convenience functions

long get_long(grib_handle *g, const char *name);
double get_double(grib_handle *g, const char *name);
void gset(grib_handle *g, const char *name, double value);
void gset(grib_handle *g, const char *name, long value);
void gset(grib_handle *g, const char *name, unsigned long value);
void gset(grib_handle *g, const char *name, int value);
void gset(grib_handle *g, const char *name, const char *value);
void gset(grib_handle *g, const char *name, const std::string &value);

// grib.conf reader

struct ParamChangeItem
{
 public:
  ParamChangeItem();
  ParamChangeItem(const ParamChangeItem &theOther);
  ~ParamChangeItem();
  ParamChangeItem &operator=(const ParamChangeItem &theOther);
  void Reset();

  long itsOriginalParamId = 0;
  NFmiParam itsWantedParam;
  float itsConversionBase = 0;  // jos ei 0 ja scale ei 1, tehdään parametrille muunnos konversio
  float itsConversionScale = 1.F;
  NFmiLevel* itsLevel = nullptr;  // jos ei 0-pointer, tehdään tästä levelistä pintaparametri
};

bool GetParamChangeItemFromString(const std::string &buffer,
                                  const std::string &theInitFileName,
                                  ParamChangeItem &theParamChangeItemOut);

std::vector<ParamChangeItem> ReadGribConf(const std::string &theParamChangeTableFileName);

// GRIB2 fallback identification entry of grib.conf:
//
//   discipline.category.number;paramId;name
//
// The id and name eccodes assigns to a GRIB2 message depend on the eccodes version and on
// local definition patches. These entries give the id and name to use for the messages
// eccodes cannot identify, keyed by values stored in the message itself.

struct Grib2FallbackParam
{
  long itsDiscipline = 0;
  long itsCategory = 0;
  long itsNumber = 0;
  long itsParamId = 0;
  std::string itsName;
};

std::vector<Grib2FallbackParam> ReadGrib2FallbackParams(const std::string &theGribConfFileName);

#endif  //  GRIBTOOLS_H

// ----------------------------------------------------------------------
