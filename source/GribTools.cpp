// ======================================================================
/*!
 * \brief GRIB handling tools
 */
// ======================================================================

#include "GribTools.h"
#include "NFmiCommentStripper.h"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <stdexcept>

// ----------------------------------------------------------------------
// Dump the given namespace attributes
// ----------------------------------------------------------------------

void DUMP(grib_handle *grib, const char *ns)
{
  if (ns == NULL)
    std::cout << "\nValues in global namespace:" << std::endl;
  else
    std::cout << "\nValues for namespace " << ns << ":" << std::endl << std::endl;

  grib_keys_iterator *kiter;
  if (ns == NULL)
    kiter = grib_keys_iterator_new(grib, GRIB_KEYS_ITERATOR_ALL_KEYS, NULL);
  else
    kiter = grib_keys_iterator_new(grib, GRIB_KEYS_ITERATOR_ALL_KEYS, const_cast<char *>(ns));

  if (!kiter) throw std::runtime_error("Failed to get iterator for grib keys");

  const int MAX_STRING_LEN = 1024;
  char buffer[MAX_STRING_LEN];

  while (grib_keys_iterator_next(kiter))
  {
    const char *name = grib_keys_iterator_get_name(kiter);
    int ret = GRIB_SUCCESS;
    if (grib_is_missing(grib, name, &ret) && ret == GRIB_SUCCESS)
    {
      std::cout << name << " = "
                << "MISSING" << std::endl;
    }
    else
    {
      int type;
      grib_get_native_type(grib, name, &type);
      switch (type)
      {
        case GRIB_TYPE_STRING:
        {
          size_t len = MAX_STRING_LEN;
          grib_get_string(grib, name, buffer, &len);
          std::cout << name << " = \"" << std::string(buffer, strlen(buffer)) << '"' << std::endl;
          break;
        }
        case GRIB_TYPE_DOUBLE:
        {
          double value;
          grib_get_double(grib, name, &value);
          std::cout << name << " = " << value << std::endl;
          break;
        }
        case GRIB_TYPE_LONG:
        {
          long value;
          grib_get_long(grib, name, &value);
          std::cout << name << " = " << value << std::endl;
          break;
        }
        default:
          std::cout << "Unknown header type in grib with name" << name << std::endl;
      }
    }
  }
  grib_keys_iterator_delete(kiter);
}

// ----------------------------------------------------------------------
// Dump all namespaces
// ----------------------------------------------------------------------

void DUMP(grib_handle *grib)
{
  DUMP(grib, NULL);
  DUMP(grib, "geography");
  DUMP(grib, "parameter");
  DUMP(grib, "time");
  DUMP(grib, "vertical");
}

// ----------------------------------------------------------------------
// Convenience functions
// ----------------------------------------------------------------------

long get_long(grib_handle *g, const char *name)
{
  long value;
  if (grib_get_long(g, name, &value))
    throw std::runtime_error(std::string("Failed to get long value for name ") + name);
  return value;
}

double get_double(grib_handle *g, const char *name)
{
  double value;
  if (grib_get_double(g, name, &value))
    throw std::runtime_error(std::string("Failed to get double value for name ") + name);
  return value;
}

void gset(grib_handle *g, const char *name, double value)
{
  if (grib_set_double(g, name, value))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " +
                             boost::lexical_cast<std::string>(value));
}

void gset(grib_handle *g, const char *name, long value)
{
  if (grib_set_long(g, name, value))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " +
                             boost::lexical_cast<std::string>(value));
}

void gset(grib_handle *g, const char *name, unsigned long value)
{
  if (grib_set_long(g, name, value))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " +
                             boost::lexical_cast<std::string>(value));
}

void gset(grib_handle *g, const char *name, int value)
{
  if (grib_set_long(g, name, value))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " +
                             boost::lexical_cast<std::string>(value));
}

void gset(grib_handle *g, const char *name, const char *value)
{
  size_t len = strlen(value);
  if (grib_set_string(g, name, value, &len))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " + value);
}

void gset(grib_handle *g, const char *name, const std::string &value)
{
  size_t len = value.size();
  if (grib_set_string(g, name, value.c_str(), &len))
    throw std::runtime_error(std::string("Failed to set ") + name + " to value " + value);
}

// ----------------------------------------------------------------------
// Parameter change item
// ----------------------------------------------------------------------

ParamChangeItem::ParamChangeItem()
    : itsOriginalParamId(0),
      itsWantedParam(0,
                     "",
                     kFloatMissing,
                     kFloatMissing,
                     kFloatMissing,
                     kFloatMissing,
                     "%.1f",
                     kLinearly)  // laitetaan lineaarinen interpolointi päälle
      ,
      itsConversionBase(0),
      itsConversionScale(1.f),
      itsLevel(0)
{
}

ParamChangeItem::ParamChangeItem(const ParamChangeItem &theOther)
    : itsOriginalParamId(theOther.itsOriginalParamId),
      itsWantedParam(theOther.itsWantedParam),
      itsConversionBase(theOther.itsConversionBase),
      itsConversionScale(theOther.itsConversionScale),
      itsLevel(theOther.itsLevel ? new NFmiLevel(*theOther.itsLevel) : 0)
{
}

ParamChangeItem::~ParamChangeItem() { delete itsLevel; }
ParamChangeItem &ParamChangeItem::operator=(const ParamChangeItem &theOther)
{
  if (this != &theOther)
  {
    itsOriginalParamId = theOther.itsOriginalParamId;
    itsWantedParam = theOther.itsWantedParam;
    itsConversionBase = theOther.itsConversionBase;
    itsConversionScale = theOther.itsConversionScale;
    itsLevel = theOther.itsLevel ? new NFmiLevel(*theOther.itsLevel) : 0;
  }
  return *this;
}

void ParamChangeItem::Reset()
{
  itsOriginalParamId = 0;
  itsWantedParam = NFmiParam(0,
                             "",
                             kFloatMissing,
                             kFloatMissing,
                             kFloatMissing,
                             kFloatMissing,
                             "%.1f",
                             kLinearly);  // laitetaan lineaarinen interpolointi päälle
  itsConversionBase = 0;
  itsConversionScale = 1.f;
  if (itsLevel) delete itsLevel;
  itsLevel = 0;
}

// ----------------------------------------------------------------------
// purkaa rivin
// origParId;wantedParId;wantedParName[;base][;scale][;levelType;level][;interpolationMethod]
// 129;4;Temperature;-273.15;1;103;2  // (here e.g. T change from kelvins to celsius and changing
// from height 2m level to surface data)
// 11;1;Pressure;0;0.01    // (here e.g. changing the pressure from pascals to mbars)
// Arvon voi jättää väleissä tyhjäksi (paitsi ei 3 ensimmaäistä), jolloin käytetään default arvoja,
// esim jos halutaan laittaa
// interpolaatio nearest tyyppiseksi:
// 129;57;PrecForm;;;;;2  // (final value 2 is the nearest interpolation)
// ----------------------------------------------------------------------

bool GetParamChangeItemFromString(const std::string &buffer,
                                  const std::string &theInitFileName,
                                  ParamChangeItem &theParamChangeItemOut)
{
  std::vector<std::string> strVector = NFmiStringTools::Split(buffer, ";");
  if (strVector.size() <= 1) return false;  // skipataan tyhjät rivit
  if (strVector.size() < 3)
  {
    std::string errStr(
        "GetParamChangeItemFromString - ParamChangeItem info had too few parts on line\n");
    errStr += buffer;
    errStr += "\n in file: ";
    errStr += theInitFileName;
    throw std::runtime_error(errStr);
  }

  theParamChangeItemOut.Reset();

  theParamChangeItemOut.itsOriginalParamId = boost::lexical_cast<long>(strVector[0]);
  long newParId = boost::lexical_cast<long>(strVector[1]);
  std::string newParName = strVector[2];
  theParamChangeItemOut.itsWantedParam.SetIdent(newParId);
  theParamChangeItemOut.itsWantedParam.SetName(newParName);
  if (strVector.size() >= 4)
  {
    if (!strVector[3].empty())
      theParamChangeItemOut.itsConversionBase = boost::lexical_cast<float>(strVector[3]);
  }
  if (strVector.size() >= 5)
  {
    if (!strVector[4].empty())
      theParamChangeItemOut.itsConversionScale = boost::lexical_cast<float>(strVector[4]);
  }

  if (strVector.size() > 5 && strVector.size() < 7)
  {
    std::string errStr(
        "GetParamChangeItemFromString - ParamChangeItem info had some level info, but there should "
        "be levelType and levelValue on line\n");
    errStr += buffer;
    errStr += "\n in file: ";
    errStr += theInitFileName;
    throw std::runtime_error(errStr);
  }

  if (strVector.size() >= 7)
  {
    if (!strVector[5].empty() && !strVector[6].empty())
    {
      unsigned long levelType = boost::lexical_cast<unsigned long>(strVector[5]);
      float levelValue = boost::lexical_cast<float>(strVector[6]);
      theParamChangeItemOut.itsLevel =
          new NFmiLevel(levelType, NFmiStringTools::Convert(levelValue), levelValue);
    }
    else if (strVector[5].empty() != strVector[6].empty())
    {
      std::string errStr(
          "GetParamChangeItemFromString - ParamChangeItem info had some level info, but there "
          "should be *both* levelType and levelValue on line\n");
      errStr += buffer;
      errStr += "\n in file: ";
      errStr += theInitFileName;
      throw std::runtime_error(errStr);
    }
  }
  if (strVector.size() == 8)
  {
    if (!strVector[7].empty())
    {
      FmiInterpolationMethod interpMethod =
          static_cast<FmiInterpolationMethod>(boost::lexical_cast<int>(strVector[7]));
      theParamChangeItemOut.itsWantedParam.InterpolationMethod(interpMethod);
    }
  }
  return true;
}

// ----------------------------------------------------------------------
// Lukee initialisointi tiedostosta parametreja koskevat muunnos tiedot.
// param tiedoston formaatti on seuraava (kommentit sallittuja, mutta itse param rivit
// ilman kommentti merkkejä) ja kenttien erottimina on ;-merkki.
//
// // origParId;wantedParId;wantedParName[;base][;scale][;levelType;level]
// 129;4;Temperature;-273.15;1;103;2  // (here e.g. T change from kelvins to celsius and changing
// from height 2m level to surface data)
// 11;1;Pressure;0;0.01    // (here e.g. changing the pressure from pascals to mbars)
// ----------------------------------------------------------------------

std::vector<ParamChangeItem> ReadGribConf(const std::string &theParamChangeTableFileName)
{
  std::vector<ParamChangeItem> paramChangeTable;

  if (theParamChangeTableFileName.empty())
    throw std::runtime_error(
        "InitParamChangeTable: empty ParamChangeTableFileName filename given.");

  NFmiCommentStripper stripComments;
  if (stripComments.ReadAndStripFile(theParamChangeTableFileName))
  {
    std::stringstream in(stripComments.GetString());

    const int maxBufferSize = 1024 + 1;  // kuinka pitkä yhden rivin maksimissaan oletetaan olevan
    std::string buffer;
    ParamChangeItem paramChangeItem;
    int i = 0;
    int counter = 0;
    do
    {
      buffer.resize(maxBufferSize);
      in.getline(&buffer[0], maxBufferSize);

      size_t realSize = strlen(buffer.c_str());
      buffer.resize(realSize);
      if (::GetParamChangeItemFromString(buffer, theParamChangeTableFileName, paramChangeItem))
      {
        counter++;
        paramChangeTable.push_back(paramChangeItem);
      }
      i++;
    } while (in.good());
  }
  else
    throw std::runtime_error(std::string("InitParamChangeTable: trouble reading file: ") +
                             theParamChangeTableFileName);

  return paramChangeTable;
}
