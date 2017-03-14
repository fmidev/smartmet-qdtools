// synop2qd.cpp Tekij‰ Marko 28.11.2006
// Lukee halutut tiedostot/tiedostonimi-filtterit, joista
// lˆytyv‰t SYNOP-koodit tulkitaan ja niist‰ muodostetaan
// querydata, miss‰ on yhdistettyn‰ kaikki tulkitut synop-havainnot.

#include <fstream>

#include <newbase/NFmiArea.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiFileSystem.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiMilliSecondTimer.h>
#include <newbase/NFmiProducerName.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiStreamQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiTotalWind.h>
#include <smarttools/NFmiAviationStationInfoSystem.h>
#include <smarttools/NFmiSoundingFunctions.h>

#include <fstream>

using namespace std;

class ExceptionSynopEndOk
{
 public:
  ExceptionSynopEndOk(int /* dummy*/)
  {  // g++ k‰‰nt‰j‰n takia pit‰‰ tehd‰ dummy konstruktori, jolle annetaan parametri
  }
};

class ExceptionSynopEndIgnoreMessage
{
};

template <typename T>
static T GetValue(const std::string &theValueStr,
                  string::size_type startPos,
                  string::size_type endPos,
                  const std::string &theSynopStr)
{
  string nilStr("nil");

  // Tarkistuksia ett‰ haettu ali stringi pysyy annetun stringin sis‰ll‰
  if (startPos >= theValueStr.size()) return static_cast<T>(kFloatMissing);
  if (endPos >= theValueStr.size()) return static_cast<T>(kFloatMissing);

  std::string tmpStr(theValueStr.begin() + startPos, theValueStr.begin() + endPos + 1);
  NFmiStringTools::LowerCase(tmpStr);
  if (tmpStr.find(nilStr) != string::npos) throw ExceptionSynopEndOk(1);
  string missingString1("/");
  string missingString2("x");
  if (endPos - startPos + 1 == 2)
  {
    missingString1 = "//";
    missingString2 = "xx";
  }
  else if (endPos - startPos + 1 == 3)
  {
    missingString1 = "///";
    missingString2 = "xxx";
  }
  else if (endPos - startPos + 1 == 4)
  {
    missingString1 = "////";
    missingString2 = "xxxx";
  }
  else if (endPos - startPos + 1 == 5)
  {
    missingString1 = "/////";
    missingString2 = "xxxxx";
  }

  if (tmpStr != missingString1 && tmpStr != missingString2)
  {
    T value = 0;
    try
    {
      value = NFmiStringTools::Convert<T>(tmpStr);
    }
    catch (exception &e)
    {
      string errReport("Error with synop:\n");
      errReport += theSynopStr;
      errReport += "\n";
      errReport += e.what();
      throw runtime_error(errReport);
    }
    return value;
  }
  else
    return static_cast<T>(kFloatMissing);
}

static float SynopVisCode2Metres(float theVisibility)
{
  if (theVisibility == kFloatMissing)
    return kFloatMissing;
  else if (theVisibility == 0)
    return 75;  // 75 metri‰ on arvaus, 00-koodi on alle 100 m
  else if (theVisibility > 0 && theVisibility <= 50)
    return theVisibility * 100;
  else if (theVisibility >= 56 && theVisibility <= 80)
    return (theVisibility - 50) * 1000;
  else if (theVisibility >= 81 && theVisibility <= 88)
    return ((theVisibility - 80) * 5000) + 30000;
  else if (theVisibility == 89)
    return 75000;  // 75000 metri‰ on arvaus, 89-koodi on yli 70000 m
  else if (theVisibility >= 51 && theVisibility <= 55)
    return 5000;  // sellaisia synoppeja on pilvin pimein, miss‰ on VV -koodi 51-55, tein niin ett‰
                  // niist‰ ei valiteta ja arvoksi laitetaan pyˆre‰t 5000 m
                  //		throw runtime_error(string("Invalid synop visibility code: ") +
  // NFmiStringTools::Convert<int>(static_cast<int>(theVisibility)));
  else if (theVisibility < 0 || theVisibility > 99)
    throw runtime_error(string("Invalid synop visibility code: ") +
                        NFmiStringTools::Convert<int>(static_cast<int>(theVisibility)));
  else
  {
    int visInt = static_cast<int>(theVisibility);
    switch (visInt)
    {
      case 90:
        return 40;
      case 91:
        return 50;
      case 92:
        return 200;
      case 93:
        return 500;
      case 94:
        return 1000;
      case 95:
        return 2000;
      case 96:
        return 4000;
      case 97:
        return 10000;
      case 98:
        return 20000;
      case 99:
        return 50000;
      default:
        throw runtime_error(string("Invalid synop visibility code or error in program: ") +
                            NFmiStringTools::Convert<int>(visInt));
    }
  }
}

class NFmiSynopCode
{
 public:
  NFmiSynopCode(void) : itsCodeStr(), itsTime(), itsStation(), itsKnownStations(0), fVerbose(false)
  {
    Clear();
  }

  NFmiSynopCode(NFmiAviationStationInfoSystem *theKnownStations, bool verbose)
      : itsCodeStr(),
        itsTime(),
        itsStation(),
        itsKnownStations(theKnownStations),
        itsDriftingStationLocation(),
        fVerbose(verbose)
  {
    Clear();
  }

  ~NFmiSynopCode(void){};

  void Clear(void)
  {
    itsCodeStr = "";

    itsTime = NFmiMetTime();
    itsStation = NFmiStation();

    itsPressure = kFloatMissing;
    itsTemperature = kFloatMissing;
    itsDewPoint = kFloatMissing;
    itsRH = kFloatMissing;
    itsWS = kFloatMissing;
    itsWD = kFloatMissing;
    itsN = kFloatMissing;
    itsVisibility = kFloatMissing;
    itsCloudBaseHeight = kFloatMissing;
    itsPressureTendency = kFloatMissing;
    itsPressureChange = kFloatMissing;
    itsPrecipitation = kFloatMissing;
    itsPresentWeatherCode = kFloatMissing;
    itsW1 = kFloatMissing;
    itsW2 = kFloatMissing;
    itsNh = kFloatMissing;
    itsCl = kFloatMissing;
    itsCm = kFloatMissing;
    itsCh = kFloatMissing;
    itsTmin = kFloatMissing;
    itsTmax = kFloatMissing;
    itsTwater = kFloatMissing;
    itsLon = kFloatMissing;
    itsLat = kFloatMissing;
    itsGust = kFloatMissing;
  }

  void ReadNextField(istream &in,
                     string &theStr,
                     const string &theCommentStr,
                     const string &theSynopStr,
                     bool fMustBeFound)
  {
    if (in.good() == false)
    {
      if (fMustBeFound)
        throw runtime_error(string("Error in NFmiSynopCode::ReadNextField with string: ") +
                            theCommentStr + " in synop:\n" + theSynopStr);
      else
        throw ExceptionSynopEndOk(1);
    }
    in >> theStr;
  }

  float GetTemperatureFromField(const string &theTfieldStr, const string &theSynopStr)
  {
    int sign = ::GetValue<int>(theTfieldStr, 1, 1, theSynopStr);
    if (::isdigit(static_cast<unsigned char>(theTfieldStr[2])) &&
        ::isdigit(static_cast<unsigned char>(theTfieldStr[3])) &&
        (theTfieldStr[4] == '/' || ::tolower(theTfieldStr[4]) == 'x'))
    {
      float T = ::GetValue<float>(theTfieldStr, 2, 3, theSynopStr);
      if (T != kFloatMissing)
        return sign ? -T : T;
      else
        return kFloatMissing;
    }
    else
    {
      float T = ::GetValue<float>(theTfieldStr, 2, 4, theSynopStr);
      if (T != kFloatMissing)
        return sign ? -T / 10.f : T / 10.f;
      else
        return kFloatMissing;
    }
  }

  float GetBuoyLatitude(const std::string &QcLaLaLaLaLa_Str,
                        const std::string &theSynopStr,
                        int &theQuadrantOfGlobe)
  {
    if (QcLaLaLaLaLa_Str.size() == 6)
    {
      theQuadrantOfGlobe = ::GetValue<int>(QcLaLaLaLaLa_Str, 0, 0, theSynopStr);
      float lat = 0.f;
      if (::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[1])) &&
          ::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[2])) &&
          ::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[3])) &&
          ::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[4])) && QcLaLaLaLaLa_Str[5] == '/')
      {
        lat = static_cast<float>(::GetValue<int>(QcLaLaLaLaLa_Str, 1, 4, theSynopStr));
        lat /= 100.f;
      }
      else if (::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[1])) &&
               ::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[2])) &&
               ::isdigit(static_cast<unsigned char>(QcLaLaLaLaLa_Str[3])) &&
               QcLaLaLaLaLa_Str[4] == '/' && QcLaLaLaLaLa_Str[5] == '/')
      {
        lat = static_cast<float>(::GetValue<int>(QcLaLaLaLaLa_Str, 1, 3, theSynopStr));
        lat /= 10.f;
      }
      else
      {
        lat = static_cast<float>(::GetValue<int>(QcLaLaLaLaLa_Str, 1, 5, theSynopStr));
        lat /= 1000.f;
      }

      return lat;
    }
    else
      throw runtime_error(string("BUOY message didn't contain correct QcLaLaLaLaLa-field:\n") +
                          theSynopStr);
  }

  float GetBuoyLongitude(const std::string &LoLoLoLoLoLo_Str, const std::string &theSynopStr)
  {
    if (LoLoLoLoLoLo_Str.size() == 6)
    {
      float lon = 0;
      if (::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[0])) &&
          ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[1])) &&
          ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[3])) &&
          ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[3])) &&
          ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[4])) && LoLoLoLoLoLo_Str[5] == '/')
      {
        lon = static_cast<float>(::GetValue<int>(LoLoLoLoLoLo_Str, 0, 4, theSynopStr));
        lon /= 100.f;
      }
      else if (::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[0])) &&
               ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[1])) &&
               ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[2])) &&
               ::isdigit(static_cast<unsigned char>(LoLoLoLoLoLo_Str[3])) &&
               LoLoLoLoLoLo_Str[4] == '/' && LoLoLoLoLoLo_Str[5] == '/')
      {
        lon = static_cast<float>(::GetValue<int>(LoLoLoLoLoLo_Str, 0, 3, theSynopStr));
        lon /= 10.f;
      }
      else
      {
        lon = static_cast<float>(::GetValue<int>(LoLoLoLoLoLo_Str, 0, 5, theSynopStr));
        lon /= 1000.f;
      }

      return lon;
    }
    else
      throw runtime_error(string("BUOY message didn't contain correct LoLoLoLoLoLo-field:\n") +
                          theSynopStr);
  }

  float GetShipLatitude(const std::string &_99LaLaLa_Str, const std::string &theSynopStr)
  {
    if (_99LaLaLa_Str.size() == 5 && _99LaLaLa_Str[0] == '9' && _99LaLaLa_Str[1] == '9')
    {
      int latCode = ::GetValue<int>(_99LaLaLa_Str, 2, 4, theSynopStr);
      float lat = static_cast<float>(latCode / 10);
      int tenthsPart = latCode % 10;
      lat += tenthsPart / 6.f;
      return lat;
    }
    else
      throw runtime_error(string("SHIP message didn't contain correct 99LaLaLa-field:\n") +
                          theSynopStr);
  }

  float GetShipLongitude(const std::string &QcLoLoLoLo_Str,
                         const std::string &theSynopStr,
                         int &theQuadrantOfGlobe)
  {
    if (QcLoLoLoLo_Str.size() == 5)
    {
      theQuadrantOfGlobe = ::GetValue<int>(QcLoLoLoLo_Str, 0, 0, theSynopStr);
      int lonCode = ::GetValue<int>(QcLoLoLoLo_Str, 1, 4, theSynopStr);
      float lon = static_cast<float>(lonCode / 10);
      int tenthsPart = lonCode % 10;
      lon += tenthsPart / 6.f;
      return lon;
    }
    else
      throw runtime_error(string("SHIP message didn't contain correct 99LaLaLa-field:\n") +
                          theSynopStr);
  }

  // puretaan 4PPPP-kentt‰ // t‰m‰ on meren pintaan redukoitu paine
  // paitsi jos ryhm‰ 3 oli annettu, ja t‰ss‰ onkin 4a3hhh
  // eli geopotentiaalin korkeus. a3 arvot ovat:
  // 1 -- 1000 mb
  // 2 -- 925 mb
  // 5 -- 500 mb
  // 7 -- 700 mb
  // 8 -- 850 mb
  float GetPressureFromField(const std::string &currentSynopField, const std::string &theSynopStr)
  {
    float P = kFloatMissing;
    if (currentSynopField[1] == '0' || currentSynopField[1] == '9')
    {
      float PPP41 = ::GetValue<float>(currentSynopField, 1, 3, theSynopStr);
      float PPP42 = ::GetValue<float>(currentSynopField, 4, 4, theSynopStr);
      if (PPP41 != kFloatMissing && PPP42 != kFloatMissing)
        P = (PPP41 < 500.f) ? (1000 + PPP41 + PPP42 / 10.f) : (PPP41 + PPP42 / 10.f);
      else if (PPP41 != kFloatMissing && PPP42 == kFloatMissing)
        P = (PPP41 < 500.f) ? (1000 + PPP41) : (PPP41);
    }
    return P;
  }

  void DecodeBouy(const std::string &theSynopStr, bool fWindSpeedInKnots, int theWordSkipCount)
  {
    try
    {
      Clear();
      stringstream ssin(theSynopStr);
      string tmpStr;
      for (int i = 0; i < theWordSkipCount; i++)
        ReadNextField(ssin, tmpStr, "skippin words in DecodeBouy", theSynopStr, true);

      string QcLaLaLaLaLa_Str;  // pakollinen
      ReadNextField(ssin, QcLaLaLaLaLa_Str, "QcLaLaLaLaLa", theSynopStr, true);
      int quadrantOfGlobe = 0;
      itsLat = GetBuoyLatitude(QcLaLaLaLaLa_Str, theSynopStr, quadrantOfGlobe);

      string LoLoLoLoLoLo_Str;  // pakollinen
      ReadNextField(ssin, LoLoLoLoLoLo_Str, "LoLoLoLoLoLo", theSynopStr, true);
      itsLon = GetBuoyLongitude(LoLoLoLoLoLo_Str, theSynopStr);
      if (quadrantOfGlobe == 7)  // north-east
        itsLon = -itsLon;
      else if (quadrantOfGlobe == 5)  // south-east
      {
        itsLon = -itsLon;
        itsLat = -itsLat;
      }
      else if (quadrantOfGlobe == 3)  // south-west
        itsLat = -itsLat;

      // asetetaan feikki arvoja poijuille, koska kaikki lasketaan ns. driftereiksi
      itsStation.SetLatitude(itsDriftingStationLocation.Y());
      itsStation.SetLongitude(itsDriftingStationLocation.X());

      string commentString = "6QiQt__ or some following field";
      string currentSynopField;
      ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      if (currentSynopField.size() == 5 && currentSynopField[0] == '6')
      {  // t‰m‰ kentt‰ skipataan ainakin toistaiseksi

        commentString = "searching 111QdQx-field or 222QdQx or some following field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '1' &&
          currentSynopField[1] == '1' && currentSynopField[2] == '1')
      {  // so there was 111QdQx-field and 111-section

        commentString = "searching 0ddff or some following field";
        ReadNextField(ssin, currentSynopField, "0ddff", theSynopStr, true);
        if (currentSynopField.size() == 5 && currentSynopField[0] == '0')
        {
          float dd = ::GetValue<float>(currentSynopField, 1, 2, theSynopStr);
          if (dd != kFloatMissing) itsWD = dd * 10.f;
          float ff = ::GetValue<float>(currentSynopField, 3, 4, theSynopStr);

          // if speed is 99, there is an extra 00fff group coming up
          if (ff == 99)
          {
            commentString = "searching 00fff or some following field";
            ReadNextField(ssin, currentSynopField, "00fff", theSynopStr, true);
            ff = ::GetValue<float>(currentSynopField, 2, 5, theSynopStr);
          }

          if (ff != kFloatMissing)
          {
            if (fWindSpeedInKnots)
              itsWS = 1852 * ff / 3600.f;
            else
              itsWS = ff;
          }

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 1sTTT-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, true);
        }

        if (currentSynopField.size() == 5 && currentSynopField[0] == '1')
        {  // puretaan 1sTTT-kentt‰
          itsTemperature = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 2sTdTdTd-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        if (currentSynopField.size() == 5 && currentSynopField[0] == '2')
        {  // puretaan 2sTdTdTd-kentt‰ / 29UUU
          if (currentSynopField[1] == '9')
            itsRH = ::GetValue<float>(currentSynopField, 2, 4, theSynopStr);
          else
            itsDewPoint = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 3P0P0P0P0-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        if (currentSynopField.size() == 5 && currentSynopField[0] == '3')
        {  // puretaan 3P0P0P0P0-kentt‰ // t‰m‰ on asemalla mitattu paine // t‰t‰ ei toistaiseksi
           // oteta talteen
#if 0
					double PPP31 = ::GetValue<double>(currentSynopField, 1, 3, theSynopStr);
					double PPP32 = ::GetValue<double>(currentSynopField, 4, 4, theSynopStr);
					double tmpPressure = 0;
					if(PPP31 != kFloatMissing && PPP32 != kFloatMissing)
						tmpPressure = (PPP31 < 500.) ? (1000 + PPP31 + PPP32/10.) : (PPP31 + PPP32/10.);
					else if(PPP31 != kFloatMissing && PPP32 == kFloatMissing)
						tmpPressure = (PPP31 < 500.) ? (1000 + PPP31) : (PPP31);
#endif

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 4PPPP-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        if (currentSynopField.size() == 5 && currentSynopField[0] == '4')
        {
          itsPressure = GetPressureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 5appp-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        if (currentSynopField.size() == 5 && currentSynopField[0] == '5')
        {  // puretaan 5appp-kentt‰
          float a = ::GetValue<float>(currentSynopField, 1, 1, theSynopStr);
          if (a != kFloatMissing) itsPressureTendency = a;
          float ppp = ::GetValue<float>(currentSynopField, 2, 4, theSynopStr);
          if (ppp != kFloatMissing) itsPressureChange = ppp / 10.f;

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 6RRRt-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }
      }  // end of 111-section

      if (currentSynopField.size() == 5 && currentSynopField[0] == '2' &&
          currentSynopField[1] == '2' && currentSynopField[2] == '2')
      {  // so there was 222QdQx-field and 222-section

        commentString = "searching 0snTwTwTw or some following field";
        ReadNextField(ssin, currentSynopField, "0snTwTwTw", theSynopStr, true);

        if (currentSynopField.size() == 5 && currentSynopField[0] == '0')
        {  // puretaan 0snTwTwTw-kentt‰
          itsTwater = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 1PwaPwaHwaHwa-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }
      }  // end of 222-section
    }
    catch (ExceptionSynopEndOk & /* e */)
    {
      // t‰m‰ poikkeus lopetus ok, poistutaan vain t‰st‰ metodista, ja laitetaan synop talteen
    }
  }

  // t‰m‰ on puuttuvien asemien listaukseen k‰ytetty debuggaus luokka
  class StationErrorStrings : public set<string>
  {
   public:
    ~StationErrorStrings(void)
    {
      ofstream out("d://data//11caribia//missing_wmo_ids.txt");
      if (out)
      {
        out << endl << "Here is list of all unknown ICAO ids:" << endl;
        StationErrorStrings::iterator it = begin();
        for (; it != end(); ++it)
          out << *it << ", ";
        out << endl << endl;
      }
    }
  };

  void Decode(const std::string &theSynopStr,
              bool fDoShipMessages,
              bool fWindSpeedInKnots,
              int theWordSkipCount,
              std::set<unsigned long> &theUnknownWmoIdsInOut)
  {
    try
    {
      Clear();
      stringstream ssin(theSynopStr);
      string tmpStr;
      for (int i = 0; i < theWordSkipCount; i++)
        ReadNextField(ssin, tmpStr, "skippin words in Decode", theSynopStr, true);

      if (fDoShipMessages)
      {
        string _99LaLaLa_Str;  // pakollinen
        ReadNextField(ssin, _99LaLaLa_Str, "99LaLaLa", theSynopStr, true);
        itsLat = GetShipLatitude(_99LaLaLa_Str, theSynopStr);

        string QcLoLoLoLo_Str;  // pakollinen
        ReadNextField(ssin, QcLoLoLoLo_Str, "QcLoLoLoLo", theSynopStr, true);
        int quadrantOfGlobe = 0;
        itsLon = GetShipLongitude(QcLoLoLoLo_Str, theSynopStr, quadrantOfGlobe);
        if (quadrantOfGlobe == 7)  // north-east
          itsLon = -itsLon;
        else if (quadrantOfGlobe == 5)  // south-east
        {
          itsLon = -itsLon;
          itsLat = -itsLat;
        }
        else if (quadrantOfGlobe == 3)  // south-west
          itsLat = -itsLat;

        // asetetaan feikki arvoja asemalle
        itsStation.SetLatitude(itsDriftingStationLocation.Y());
        itsStation.SetLongitude(itsDriftingStationLocation.X());
        // itsStation.SetName(IIiii_Str); // laivojen tapauksessa aseman nimi annetaan t‰m‰n
        // funktion ulkopuolella
        itsStation.SetIdent(0);
      }
      else  // normal synop
      {
        string IIiii_Str;  // pakollinen
        ReadNextField(ssin, IIiii_Str, "IIiii", theSynopStr, true);

        if (IIiii_Str.size() != 5)
          throw std::invalid_argument("Expecting section 0 string of form 'IIiii', got " +
                                      IIiii_Str + " instead");

        if (::isdigit(static_cast<unsigned char>(IIiii_Str[0])) == false)
          //			throw runtime_error(string("Ignoring this synop, starts with non
          // digit
          // value, might be header part: \n") + theSynopStr);
          throw ExceptionSynopEndIgnoreMessage();

        if (itsKnownStations)
        {
          unsigned long wmoID = ::GetValue<unsigned long>(IIiii_Str, 0, 4, theSynopStr);
          NFmiAviationStation *aviationStation = itsKnownStations->FindStation(wmoID);
          if (aviationStation)
          {
            itsStation = NFmiStation(*aviationStation);
          }
          else
          {
            theUnknownWmoIdsInOut.insert(wmoID);
          }
        }
      }
      string iihVV_Str;  // pakollinen
      ReadNextField(ssin, iihVV_Str, "iihVV", theSynopStr, false);

      if (iihVV_Str.size() != 5)
        throw std::invalid_argument("Expecting section 1 string of form 'iihVV', got " + iihVV_Str +
                                    " instead");

      NFmiStringTools::LowerCase(iihVV_Str);
      if (iihVV_Str == string("nil")) throw ExceptionSynopEndIgnoreMessage();
      //		double iR = ::GetValue<double>(iihVV_Str, 0, 0);
      //		double ix = ::GetValue<double>(iihVV_Str, 1, 1);
      itsCloudBaseHeight = ::GetValue<float>(iihVV_Str, 2, 2, theSynopStr);
      itsVisibility = ::GetValue<float>(iihVV_Str, 3, 4, theSynopStr);
      try
      {
        itsVisibility = ::SynopVisCode2Metres(itsVisibility);
      }
      catch (exception &e)
      {
        itsVisibility = kFloatMissing;
        if (fVerbose)
        {
          cerr << "Warning: NFmiSynopCode::Decode - " << e.what() << endl;
          cerr << "In synop message: \n" << theSynopStr << endl;
        }
      }

      string Nddff_Str;  // pakollinen
      ReadNextField(ssin, Nddff_Str, "Nddff", theSynopStr, true);

      if (Nddff_Str.size() != 5)
        throw std::invalid_argument("Expecting section 1 string of form 'Nddff', got " + Nddff_Str +
                                    " instead");

      itsN = ::GetValue<float>(Nddff_Str, 0, 0, theSynopStr);
      float dd = ::GetValue<float>(Nddff_Str, 1, 2, theSynopStr);
      if (dd != kFloatMissing) itsWD = dd * 10.f;
      float ff = ::GetValue<float>(Nddff_Str, 3, 4, theSynopStr);

      // if speed is 99, there is an extra 00fff group coming up
      if (ff == 99)
      {
        ReadNextField(ssin, Nddff_Str, "00fff", theSynopStr, true);
        ff = ::GetValue<float>(Nddff_Str, 2, 5, theSynopStr);
      }

      if (ff != kFloatMissing)
      {
        if (fWindSpeedInKnots)
          itsWS = 1852 * ff / 3600.f;
        else
          itsWS = ff;
      }

      string commentString;
      string currentSynopField;

      commentString = "next trying to read 1sTTT -field";
      ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);

      if (currentSynopField.size() == 5 && currentSynopField[0] == '1')
      {  // puretaan 1sTTT-kentt‰
        itsTemperature = GetTemperatureFromField(currentSynopField, theSynopStr);

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 2sTdTdTd-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '2')
      {  // puretaan 2sTdTdTd-kentt‰ / 29UUU
        if (currentSynopField[1] == '9')
          itsRH = ::GetValue<float>(currentSynopField, 2, 4, theSynopStr);
        else
        {
          try
          {
            itsDewPoint = GetTemperatureFromField(currentSynopField, theSynopStr);
          }
          catch (...)
          {  // joskus tulee invaliideja 20/05 tms kentti‰, jotka pit‰‰ vain ohittaa
          }
        }

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 3P0P0P0P0-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '3')
      {  // puretaan 3P0P0P0P0-kentt‰ // t‰m‰ on asemalla mitattu paine // t‰t‰ ei toistaiseksi
         // oteta talteen
#if 0
				double PPP31 = kFloatMissing;
				try
				{
					PPP31 = ::GetValue<double>(currentSynopField, 1, 3, theSynopStr);
				}
				catch(...)
				{ // joskus tulee invaliideja 30/// kentti‰, jotka pit‰‰ vain ohittaa
				}
				double PPP32 = ::GetValue<double>(currentSynopField, 4, 4, theSynopStr);
				double tmpPressure = kFloatMissing;
				if(PPP31 != kFloatMissing && PPP32 != kFloatMissing)
					tmpPressure = (PPP31 < 500.) ? (1000 + PPP31 + PPP32/10.) : (PPP31 + PPP32/10.);
				else if(PPP31 != kFloatMissing && PPP32 == kFloatMissing)
					tmpPressure = (PPP31 < 500.) ? (1000 + PPP31) : (PPP31);
#endif

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 4PPPP-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '4')
      {
        itsPressure = GetPressureFromField(currentSynopField, theSynopStr);

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 5appp-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '5')
      {  // puretaan 5appp-kentt‰
        float a = ::GetValue<float>(currentSynopField, 1, 1, theSynopStr);
        if (a != kFloatMissing) itsPressureTendency = a;
        float ppp = ::GetValue<float>(currentSynopField, 2, 4, theSynopStr);
        if (ppp != kFloatMissing) itsPressureChange = ppp / 10.f;

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 6RRRt-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '6')
      {  // puretaan 6RRRt-kentt‰
        float RRR = ::GetValue<float>(currentSynopField, 1, 3, theSynopStr);
        if (RRR != kFloatMissing)
        {
          itsPrecipitation = RRR;
          if (itsPrecipitation < 0)
            throw runtime_error(
                string("Error in NFmiSynopCode::Decode, RRR-code was negative value in synop:\n") +
                theSynopStr);
          else if (itsPrecipitation >= 990)
            itsPrecipitation = (static_cast<int>(itsPrecipitation) % 10) / 10.f;
        }
        //			double t = ::GetValue<double>(currentSynopField, 4, 4); // t(r) on
        // Duration of period of precip, ei k‰ytet‰

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 7wwWW-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '7')
      {  // puretaan 7wwWW-kentt‰
        itsPresentWeatherCode = ::GetValue<float>(currentSynopField, 1, 2, theSynopStr);
        itsW1 = ::GetValue<float>(currentSynopField, 3, 3, theSynopStr);
        itsW2 = ::GetValue<float>(currentSynopField, 4, 4, theSynopStr);

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 8NhClCmCh-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '8')
      {  // puretaan 8NhClCmCh-kentt‰
        itsNh = ::GetValue<float>(currentSynopField, 1, 1, theSynopStr);
        itsCl = ::GetValue<float>(currentSynopField, 2, 2, theSynopStr);
        itsCm = ::GetValue<float>(currentSynopField, 3, 3, theSynopStr);
        itsCh = ::GetValue<float>(currentSynopField, 4, 4, theSynopStr);

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 9GGgg-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      if (currentSynopField.size() == 5 && currentSynopField[0] == '9')
      {  // 9GGgg kentt‰ skipataan

        // luetaan lopuksi seuraava kentt‰
        commentString = "next trying to read 333-field";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
      }

      // tutkitaan onko sanomassa 222-sectionia
      if (currentSynopField.size() == 5 && currentSynopField[0] == '2' &&
          currentSynopField[1] == '2' && currentSynopField[2] == '2')
      {  // so there was 222DSvS-field and 222-section

        commentString = "searching 0snTwTwTw or some following field";
        ReadNextField(ssin, currentSynopField, "0snTwTwTw", theSynopStr, true);

        if (currentSynopField.size() == 5 && currentSynopField[0] == '0')
        {  // puretaan 0snTwTwTw-kentt‰
          itsTwater = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 1PwaPwaHwaHwa-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }
      }  // end of 222-section

      // tutkitaan onko sanomassa 333-sectiota
      if (currentSynopField != string("333"))
      {
        commentString = "next trying to find 333-field";
        do
        {
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        } while (currentSynopField != string("333"));
      }

      // k‰yd‰‰n tarvittaessa l‰pi 333-sectionin kohtia
      if (currentSynopField == string("333"))
      {
        // luetaan 333:sta seuraava kentt‰
        commentString = "next trying to read 1SnTxTxTx-field from 333-section";
        ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);

        if (currentSynopField.size() == 5 && currentSynopField[0] == '1')
        {  // puretaan 1SnTxTxTx-kentt‰
          itsTmax = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          commentString = "next trying to read 2SnTnTnTn-field";
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        commentString = "next trying to read 910xx/911xx";

        if (currentSynopField.size() == 5 && currentSynopField[0] == '2')
        {
          // puretaan 1SnTnTnTn-kentt‰
          itsTmin = GetTemperatureFromField(currentSynopField, theSynopStr);

          // luetaan lopuksi seuraava kentt‰
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }

        while (ssin.good())
        {
          string prefix = currentSynopField.substr(0, 3);
          if (prefix == "910" && currentSynopField.size() == 5)
          {
            // We'll use 910ff if it is available
            itsGust = ::GetValue<float>(currentSynopField, 3, 4, theSynopStr);
            if (itsGust != kFloatMissing && fWindSpeedInKnots) itsGust = 1852 * itsGust / 3600.f;
          }
          else if (prefix == "911" && currentSynopField.size() == 5)
          {
            // We prefer 911ff over 910ff
            itsGust = ::GetValue<float>(currentSynopField, 3, 4, theSynopStr);
            if (itsGust != kFloatMissing && fWindSpeedInKnots) itsGust = 1852 * itsGust / 3600.f;
          }
          ReadNextField(ssin, currentSynopField, commentString, theSynopStr, false);
        }
      }
    }
    catch (ExceptionSynopEndOk & /* e */)
    {
      // t‰m‰ poikkeus lopetus ok, poistutaan vain t‰st‰ metodista, ja laitetaan synop talteen
    }
  }

  void Time(const NFmiMetTime &theTime) { itsTime = theTime; }
  const NFmiMetTime &Time(void) const { return itsTime; }
  const NFmiStation &Station(void) const { return itsStation; }
  // private:
  std::string itsCodeStr;

  NFmiMetTime itsTime;
  NFmiStation itsStation;
  NFmiAviationStationInfoSystem *itsKnownStations;  // Ei omista.  synop asemien "tietokanta"
  NFmiPoint itsDriftingStationLocation;  // laivoille ja poijuille annetaan feikki lokaatio, koska
                                         // niiden sijainnit vaihtuvat ja sijainnin saa datasta (lat
                                         // ja lon parametrit)
  bool fVerbose;

  float itsPressure;
  float itsTemperature;
  float itsDewPoint;
  float itsRH;
  float itsWS;
  float itsWD;
  float itsN;
  float itsVisibility;
  float itsCloudBaseHeight;
  float itsPressureTendency;
  float itsPressureChange;
  float itsPrecipitation;       // in mm
  float itsPresentWeatherCode;  // synop ww-code
  float itsW1;                  // synop W1-code
  float itsW2;                  // synop W2-code
  float itsNh;
  float itsCl;
  float itsCm;
  float itsCh;
  float itsTmin;
  float itsTmax;
  float itsTwater;
  float itsLon;                    // SHIP sanomien purussa tarvitaan sijaintia
  float itsLat;                    // SHIP sanomien purussa tarvitaan sijaintia
  float itsGust;                   // European region wind gust extension, code 910xx or 911xx
  std::string itsCorrectionField;  // Jos sanoma kuuluu johonkin korjaus blokkiin, t‰h‰n tulee
                                   // korjauksen koodi esim. RRP
};

static void Usage(void);

static std::string GetMaxNCharsFromStart(const std::string &theStr, size_t kMaxCharsCount)
{
  return std::string(
      theStr.begin(),
      (kMaxCharsCount < theStr.size()) ? theStr.begin() + kMaxCharsCount : theStr.end());
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage(void)
{
  cerr << "Usage: synop2qd [options] fileFilter1[,fileFilter2,...] > output" << endl
       << endl
       << "OBS! Doesn't work with fileFilters but just file-names yet!" << endl
       << "Program reads all the files/filefilter files given as arguments" << endl
       << "and tries to find SYNOP messages from them." << endl
       << "All the found synops are combined in one output querydata file." << endl
       << endl
       << "Options:" << endl
       << endl
       << "\t-s stationInfoFile\t File that contains different synop station infos." << endl
       << "\t-f use wmo flat-table format." << endl
       << "\t-p <1001,SYNOP or 1002,SHIP or 1017,BUOY>\tSet producer id and name." << endl
       << "\t-p <1001,SYNOP or 1007,SHIP>\tMake result datas producer id and name as wanted."
       << endl
       << "\t-t \tPut synops times to nearest synoptic times (=>3h resolution)." << endl
       << "\t-v \tVerbose mode, reports more about errors encountered." << endl
       << "\t-S \tUse this to convert SHIP-messages to qd (not with B-option)." << endl
       << "\t-B \tUse this to convert BUOY-messages to qd (not with S-option)." << endl
       << endl
       << "Note: qdconversion comes with a SYNOP stations file stored in" << endl
       << endl
       << "    /usr/share/smartmet/stations.csv" << endl
       << endl;
}

struct PointerDestroyer
{
  template <typename T>
  void operator()(T *thePtr)
  {
    delete thePtr;
  }
};

static NFmiTimeList MakeTimeList(std::set<NFmiMetTime> &theTimes)
{
  NFmiTimeList times;
  std::set<NFmiMetTime>::iterator it = theTimes.begin();
  for (; it != theTimes.end(); ++it)
    times.Add(new NFmiMetTime(*it));
  return times;
}

struct TimeCollector
{
  void operator()(const NFmiSynopCode &theSynopCode) { itsTimes.insert(theSynopCode.Time()); }
  std::set<NFmiMetTime> itsTimes;
};

struct LocationCollector
{
  void operator()(const NFmiSynopCode &theSynopCode)
  {
    itsLocations.insert(theSynopCode.Station());
  }

  std::set<NFmiStation> itsLocations;
};

static NFmiTimeList MakeTimeListForSynop(const std::vector<NFmiSynopCode> &theSynops)
{
  TimeCollector collector;
  collector = std::for_each(theSynops.begin(), theSynops.end(), TimeCollector());

  return MakeTimeList(collector.itsTimes);
}

static NFmiLocationBag MakeLocationBag(std::set<NFmiStation> &theStations, bool fDoShipMessages)
{
  unsigned long shipMessageStationId =
      122000;  // t‰m‰ on vain jokin alkuid arvo jota kasvatetaan jokaiselle eri laivalle
  NFmiLocationBag locations;
  //	std::set<T>::iterator it = theStations.begin();
  std::set<NFmiStation>::iterator it = theStations.begin();
  unsigned long ind = 0;
  std::set<NFmiPoint> points;
  std::vector<unsigned long> equalLocationIndexies;
  for (int index = 0; it != theStations.end(); ++it, index++)
  {
    NFmiStation &station = const_cast<NFmiStation &>(
        *it);  // g++ k‰‰nt‰j‰n takia pit‰‰ tehd‰ t‰ll‰isi‰ const_cast kikkkailuja
    unsigned long s1 = static_cast<unsigned long>(points.size());
    points.insert(station.GetLocation());
    unsigned long s2 = static_cast<unsigned long>(points.size());
    if (s1 == s2)
    {
      // t‰m‰ on pikaviritys. Jos arvottu luotaus-asemat, niill‰ on samat
      // sijainnit, t‰ss‰ pit‰‰ tehd‰ sijainteihin pienet erot
      ind++;
      station.SetLongitude(station.GetLongitude() + ind);
      if (fDoShipMessages) station.SetIdent(shipMessageStationId++);
    }
    else if (fDoShipMessages && index == 0)
      station.SetIdent(
          shipMessageStationId++);  // ekalle ship asemalle pit‰‰ tehd‰ id muunnos v‰kisin

    locations.AddLocation(*it);
  }

  return locations;
}

static NFmiLocationBag MakeLocationBagForSynop(const std::vector<NFmiSynopCode> &theSynops,
                                               bool fDoShipMessages)
{
  LocationCollector collector;
  collector = std::for_each(theSynops.begin(), theSynops.end(), LocationCollector());

  return MakeLocationBag(collector.itsLocations, fDoShipMessages);
}

static NFmiParamBag MakeSynopParamBag(const NFmiProducer &theWantedProducer,
                                      bool fDoShipMessages,
                                      bool fDoBuoyMessages)
{
  NFmiParamBag params;
  params.Add(NFmiDataIdent(NFmiParam(kFmiPressure, "P"), theWantedProducer));
  params.Add(NFmiDataIdent(NFmiParam(kFmiTemperature, "T"), theWantedProducer));
  params.Add(NFmiDataIdent(NFmiParam(kFmiDewPoint, "Td"), theWantedProducer));
  params.Add(NFmiDataIdent(NFmiParam(kFmiHumidity, "RH"), theWantedProducer));
  NFmiTotalWind totWind;
  NFmiDataIdent *newDataIdent = totWind.CreateParam(theWantedProducer);
  std::auto_ptr<NFmiDataIdent> newDataIdentPtr(newDataIdent);
  params.Add(*newDataIdent);
  params.Add(NFmiDataIdent(NFmiParam(kFmiPressureTendency, "a"), theWantedProducer));
  params.Add(NFmiDataIdent(NFmiParam(kFmiPressureChange, "ppp"), theWantedProducer));
  if (fDoBuoyMessages == false)
  {
    params.Add(NFmiDataIdent(NFmiParam(kFmiVisibility, "VV"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiCloudHeight, "h"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiTotalCloudCover, "N"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiPastWeather1, "W1"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiPastWeather2, "W2"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiPrecipitationAmount, "rr"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiPresentWeather, "ww"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiLowCloudCover, "Nh"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiLowCloudType, "Cl"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiMiddleCloudType, "Cm"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiHighCloudType, "Ch"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiMaximumTemperature, "Tmax"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiMinimumTemperature, "Tmin"), theWantedProducer));
  }
  if (fDoShipMessages || fDoBuoyMessages)
  {
    params.Add(NFmiDataIdent(NFmiParam(kFmiTemperatureSea, "Tw"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiLongitude, "lon"), theWantedProducer));
    params.Add(NFmiDataIdent(NFmiParam(kFmiLatitude, "lat"), theWantedProducer));
  }
  params.Add(NFmiDataIdent(NFmiParam(kFmiHourlyMaximumGust, "WG"), theWantedProducer));
  return params;
}

static NFmiQueryInfo *MakeNewInnerInfoForSYNOP(const std::vector<NFmiSynopCode> &theSynops,
                                               const NFmiProducer &theWantedProducer,
                                               bool fDoShipMessages,
                                               bool fDoBuoyMessages)
{
  NFmiQueryInfo *info = 0;
  if (theSynops.size() > 0)
  {
    NFmiTimeList times(MakeTimeListForSynop(theSynops));
    NFmiMetTime origTime;
    NFmiTimeDescriptor timeDesc(origTime, times);

    NFmiLocationBag locations(MakeLocationBagForSynop(theSynops, fDoShipMessages));
    NFmiHPlaceDescriptor hPlaceDesc(locations);

    NFmiParamBag params(MakeSynopParamBag(theWantedProducer, fDoShipMessages, fDoBuoyMessages));
    NFmiParamDescriptor paramDesc(params);

    NFmiVPlaceDescriptor vPlaceDesc;

    info = new NFmiQueryInfo(paramDesc, timeDesc, hPlaceDesc, vPlaceDesc);
  }
  return info;
}

static void CheckTimeValue(short timeValue,
                           short minValue,
                           short maxValue,
                           const std::string &timeValueName,
                           const std::string &origTimeStr,
                           const std::string &functionName)
{
  if (timeValue < minValue || timeValue > maxValue)
    throw std::runtime_error(std::string("Error in ") + functionName +
                             ": TimeString had incorrect " + timeValueName + " value '" +
                             NFmiStringTools::Convert(timeValue) + "' in time-string \"" +
                             origTimeStr + "\"");
}

static const std::string g_dayNameStr = "day";
static const std::string g_hourNameStr = "hour";

static void CheckDayValue(short dayValue,
                          const std::string &origTimeStr,
                          const std::string &functionName)
{
  ::CheckTimeValue(dayValue, 1, 31, g_dayNameStr, origTimeStr, functionName);
}

static void CheckHourValue(short hourValue,
                           const std::string &origTimeStr,
                           const std::string &functionName)
{
  ::CheckTimeValue(hourValue, 0, 23, g_hourNameStr, origTimeStr, functionName);
}

static NFmiMetTime GetTimeFromSynopHeader(const string &theTimeStr)
{
  if (theTimeStr.size() != 5)
  {
    if (!(theTimeStr.size() == 6 && ::isdigit(static_cast<unsigned char>(theTimeStr[4])) &&
          ::isdigit(static_cast<unsigned char>(theTimeStr[5]))))  // antaa tietyiss‰ tapauksissa
      // menn‰ v‰h‰n virheellinen aika
      // ruhm‰ l‰pi, eli jos on 5 merkin
      // sijasta 6, mutta lopun jutut on
      // numeroita, niin menkˆˆn
      throw runtime_error(
          string("Error in GetTimeFromSynopHeader: TimeString was not 5 characters long: '") +
          theTimeStr + "'");
  }
  short timeStep = 60;
  NFmiMetTime currentTime(timeStep);
  NFmiMetTime aTime(currentTime);
  short day = NFmiStringTools::Convert<short>(string(theTimeStr.begin(), theTimeStr.begin() + 2));
  short hour =
      NFmiStringTools::Convert<short>(string(theTimeStr.begin() + 2, theTimeStr.begin() + 4));
  ::CheckDayValue(day, theTimeStr, __FUNCTION__);
  ::CheckHourValue(hour, theTimeStr, __FUNCTION__);
  aTime.SetDay(day);
  aTime.SetHour(hour);
  aTime.SetTimeStep(timeStep);
  if (aTime > currentTime)
    aTime.PreviousMetTime(NFmiTimePerioid(
        0, 1, 0, 0, 0, 0));  // siirret‰‰n kuukausi taaksep‰in, jos saatu aika oli tulevaisuudessa
  return aTime;
}
//                                                     HHmmw                       DDMMY
static NFmiMetTime GetTimeFromBuoyHeader(const string &theTimeStr, const string &theDateStr)
{
  if (theTimeStr.size() != 5)
    throw runtime_error(
        string("Error in GetTimeFromSynopHeader: TimeString was not 5 characters long: '") +
        theTimeStr + "'");
  short day = NFmiStringTools::Convert<short>(string(theDateStr.begin(), theDateStr.begin() + 2));
  short month =
      NFmiStringTools::Convert<short>(string(theDateStr.begin() + 2, theDateStr.begin() + 4));
  short yearReminder =
      NFmiStringTools::Convert<short>(string(theDateStr.begin() + 4, theDateStr.begin() + 5));
  short timeStep = 60;
  NFmiMetTime currentTime(timeStep);
  NFmiMetTime aTime(currentTime);
  short year = aTime.GetYear();
  year /= 10;
  year *= 10;
  year = year + yearReminder;

  short hour = NFmiStringTools::Convert<short>(string(theTimeStr.begin(), theTimeStr.begin() + 2));

  ::CheckDayValue(day, theDateStr, __FUNCTION__);
  ::CheckHourValue(hour, theTimeStr, __FUNCTION__);

  NFmiMetTime aTime2(year, month, day, hour);  // tehd‰‰n nyt kokonais tunteja vain toistaiseksi
  if (aTime2 > currentTime)
    aTime2.PreviousMetTime(NFmiTimePerioid(
        10, 0, 0, 0, 0, 0));  // siirret‰‰n 10 vuotta taaksep‰in, jos saatu aika oli tulevaisuudessa
  return aTime2;
}

static void FillMissingHumidityValues(NFmiFastQueryInfo &theInfo)
{
  if (theInfo.Param(kFmiTemperature) == false) return;
  unsigned long T_ind = theInfo.ParamIndex();
  if (theInfo.Param(kFmiDewPoint) == false) return;
  unsigned long Td_ind = theInfo.ParamIndex();
  if (theInfo.Param(kFmiHumidity) == false) return;
  unsigned long RH_ind = theInfo.ParamIndex();
  float T = kFloatMissing;
  float Td = kFloatMissing;
  float RH = kFloatMissing;
  for (theInfo.ResetLevel(); theInfo.NextLevel();)
  {
    for (theInfo.ResetLocation(); theInfo.NextLocation();)
    {
      for (theInfo.ResetTime(); theInfo.NextTime();)
      {
        theInfo.ParamIndex(RH_ind);
        RH = theInfo.FloatValue();
        if (RH == kFloatMissing)
        {
          theInfo.ParamIndex(T_ind);
          T = theInfo.FloatValue();
          theInfo.ParamIndex(Td_ind);
          Td = theInfo.FloatValue();
          if (T != kFloatMissing && Td != kFloatMissing)
          {
            theInfo.ParamIndex(RH_ind);
            theInfo.FloatValue(static_cast<float>(NFmiSoundingFunctions::CalcRH(T, Td)));
          }
        }
      }
    }
  }
}

// poistaa alusta ja lopusta spacet, rivinvaihdot ja tabulaattorit
static std::string TrimFromExtraSpaces(const std::string &theStr)
{
  std::string tmp(theStr);

  // trimmataan stringin alusta ja lopusta whitespacet pois
  NFmiStringTools::Trim(tmp, ' ');
  NFmiStringTools::Trim(tmp, '\t');
  NFmiStringTools::Trim(tmp, '\t');
  NFmiStringTools::Trim(tmp, '\n');
  return tmp;
}

static std::string TakeAwayKnownErrorWordFromStart(const std::string &theStr)
{
  // tiedet‰‰n ett‰ sanomiin voi p‰‰st‰ mm. 3D sana alkuun, joka pit‰‰ vain ottaa pois synop
  // sanomasta
  string badWord("3D");
  size_t pos = theStr.find(badWord);
  if (pos == 0)
    return string(theStr.begin() + badWord.size(), theStr.end());
  else
    return theStr;
}

static void MakeSynopCodeDataFromSYNOPStr(const string &theUsedSynopBlock,
                                          NFmiAviationStationInfoSystem &theAviStations,
                                          bool fRoundTimesToNearestSynopticTimes,
                                          bool verbose,
                                          bool fDoShipMessages,
                                          bool fDoBuoyMessages,
                                          std::vector<NFmiSynopCode> &theSynopCodeVec,
                                          std::set<unsigned long> &theUnknownWmoIdsInOut,
                                          bool fUsePossibleSynopTime,
                                          const NFmiMetTime &thePossibleSynoptime,
                                          const std::string &theCorrectionField)
{
  bool doNormalTime = fDoBuoyMessages || !fUsePossibleSynopTime;
  stringstream ssin(theUsedSynopBlock);
  string shipName;
  if (fDoShipMessages || fDoBuoyMessages) ssin >> shipName;  // laivan/poijun nimi/koodi
  string dateStr;
  if (fDoBuoyMessages)
    ssin >> dateStr;  // YYMMJ  (YY p‰iv‰, MM on kuukausi ja J on vuosi (2007 = 7))
  string timeStr;
  if (doNormalTime)
    ssin >> timeStr;  // DDHHiw   (iw on tuulen nopeus indikaattori 0,1 = m/s 3,4= knots)
  NFmiMetTime aTime;
  if (!doNormalTime)
    aTime = thePossibleSynoptime;
  else
  {
    try
    {
      if (fDoBuoyMessages)
        aTime = ::GetTimeFromBuoyHeader(timeStr, dateStr);
      else
        aTime = ::GetTimeFromSynopHeader(timeStr);
    }
    catch (...)
    {
      return;
    }
  }

  size_t windIndicatorPos = (timeStr.size() == 5 ? 4 : 5);
  int iw = 4;  // defaulttina tuulen nopeus on solmuja
  if (doNormalTime)
  {
    try
    {
      iw = ::GetValue<int>(timeStr, windIndicatorPos, windIndicatorPos, timeStr);
    }
    catch (...)
    {
      if (verbose)
        cerr << std::string("Problem with time-string in synop header:\n") + timeStr +
                    "\nWill use knots as wind indicator."
             << endl;
    }
  }
  bool windSpeedInKnots = true;
  if (iw == 0 || iw == 1) windSpeedInKnots = false;

  // timeStrin sana pit‰‰ viel‰ skipata alusta
  size_t timeStrPos = theUsedSynopBlock.find(timeStr);
  if (timeStrPos == string::npos)
    throw runtime_error(
        "Error in Program, time string place not found in MakeNewDataFromSYNOPStr-function.");
  size_t startPosXX = timeStrPos + timeStr.size() + 1;
  string reallyUsedSynopBlock;
  if (startPosXX < theUsedSynopBlock.size())
    reallyUsedSynopBlock = string(theUsedSynopBlock.begin() + startPosXX, theUsedSynopBlock.end());
  // synopit on eroteltu =-merkeill‰, joten irroitetaan ne toisistaan
  std::vector<std::string> codeParcels = NFmiStringTools::Split(reallyUsedSynopBlock, "=");

  NFmiSynopCode synopCode(&theAviStations, verbose);
  synopCode.itsCorrectionField = theCorrectionField;
  int errorCount = 0;
  int decodeCount = 0;
  for (int i = 0; i < static_cast<int>(codeParcels.size()); i++)
  {
    try
    {
      string tmpStr = codeParcels[i];
      tmpStr = ::TakeAwayKnownErrorWordFromStart(tmpStr);
      tmpStr = ::TrimFromExtraSpaces(tmpStr);
      if (tmpStr.empty()) continue;
      int wordSkipCount = 0;
      if ((fDoShipMessages || fDoBuoyMessages) && i > 0)
      {
        stringstream ssin2(tmpStr);
        string shipName2;
        ssin2 >> shipName2;
        string dateStr2;
        if (fDoBuoyMessages)
          ssin >> dateStr2;  // YYMMJ  (YY p‰iv‰, MM on kuukausi ja J on vuosi (2007 = 7))
        string timeStr2;
        ssin2 >> timeStr2;  // DDHHiw   (iw on tuulen nopeus indikaattori 0,1 = m/s 3,4= knots)
        NFmiMetTime aTime2;
        try
        {
          if (fDoBuoyMessages)
            aTime2 = ::GetTimeFromBuoyHeader(timeStr2, dateStr2);
          else
            aTime2 = ::GetTimeFromSynopHeader(timeStr2);
        }
        catch (std::exception &e)
        {
          if (verbose)
          {
            std::cerr << e.what() << std::endl;
            std::cerr << "In following code: " << codeParcels[i] << std::endl;
          }
          continue;
        }
        catch (...)
        {
          continue;
        }
        if (shipName == shipName2 && timeStr == timeStr2)
          continue;  // n‰m‰ on jotain ihme monesti roportissa olevia samoilta laivoilta tulevia
                     // sanomia
        shipName = shipName2;
        aTime = aTime2;
        wordSkipCount =
            fDoBuoyMessages
                ? 3
                : 2;  // t‰ss‰ tapauksessa Decode-metodiasa pit‰‰ skipata kaksi ensimm‰ist‰ sanaa
      }
      if (fDoBuoyMessages)
        synopCode.DecodeBouy(tmpStr, windSpeedInKnots, wordSkipCount);
      else
        synopCode.Decode(
            tmpStr, fDoShipMessages, windSpeedInKnots, wordSkipCount, theUnknownWmoIdsInOut);
      synopCode.itsTime = aTime;  // t‰ss‰ blokissa on synopit aina samassa ajassa
      if (fDoShipMessages || fDoBuoyMessages) synopCode.itsStation.SetName(shipName);
      if (fDoBuoyMessages)
        synopCode.itsStation.SetIdent(NFmiStringTools::Convert<unsigned long>(shipName));
      decodeCount++;
      if (fRoundTimesToNearestSynopticTimes)
      {
        NFmiMetTime aTime(synopCode.Time());
        aTime.SetTimeStep(180);
        synopCode.Time(aTime);
      }

      // Do not save stations with unknown location info

      bool station_is_valid = (synopCode.Station().GetLongitude() != kFloatMissing &&
                               synopCode.Station().GetLatitude() != kFloatMissing);

      if (station_is_valid) theSynopCodeVec.push_back(synopCode);
    }
    catch (ExceptionSynopEndIgnoreMessage & /* e */)
    {
      // t‰m‰ oli normaali synop koodin lopetus, eli ei raporttia, mutta ei laiteta talteen
      // sanoamaa, koska se oli NIL tms.
    }
    catch (std::exception &e)
    {
      errorCount++;
      if (verbose) cerr << e.what() << endl;
    }
    catch (...)
    {
      errorCount++;
    }
  }
}

static bool StringContainsOnlyDigits(const std::string &str)
{
  for (size_t i = 0; i < str.size(); i++)
  {
    if (str[i] < '0' || str[i] > '9') return false;
  }
  return true;
}

// Oletus: stringist‰ on poistettu turhat whitespacet alusta, lopusta ja mahdollisten sanojen
// v‰lill‰ on aina vain yksi space.
// pair:in first:issa on toiseksi viimeinen sana ja second:issa on viimeinen sana (myˆs
// mahdollisesti ainoa sana)
static std::pair<std::string, std::string> GetLastTwoWords(const std::string &str)
{
  if (!str.empty())
  {
    string::size_type pos = str.find_last_of(' ');
    if (pos != string::npos)
    {
      string lastWordStr(str.begin() + pos, str.end());
      lastWordStr = ::TrimFromExtraSpaces(lastWordStr);
      if (pos > 0)
      {
        pos--;
        string::size_type pos2 = str.find_last_of(' ', pos);
        if (pos2 != string::npos)
        {
          string secondLastWordStr(str.begin() + pos2, str.begin() + pos);
          secondLastWordStr = ::TrimFromExtraSpaces(secondLastWordStr);
          return std::pair<std::string, std::string>(secondLastWordStr, lastWordStr);
        }
      }
      else
        return std::pair<std::string, std::string>("", lastWordStr);
    }
  }
  return std::pair<std::string, std::string>();
}

static bool GetPossibleSynopTime(const std::string &timeStr, NFmiMetTime &theSynopTime)
{
  if (timeStr.size() == 6)
  {
    if (StringContainsOnlyDigits(timeStr))
    {
      string dayStr(timeStr.begin(), timeStr.begin() + 2);
      string hourStr(timeStr.begin() + 2, timeStr.begin() + 4);
      string minuteStr(timeStr.begin() + 4, timeStr.begin() + 6);
      short day = NFmiStringTools::Convert<short>(dayStr);
      short hour = NFmiStringTools::Convert<short>(hourStr);
      short minute = NFmiStringTools::Convert<short>(minuteStr);
      if (day > 0 && day <= 31 && hour >= 0 && hour < 24 && minute >= 0 && minute < 60)
      {
        short timeStep = 60;
        NFmiMetTime currentTime(timeStep);
        theSynopTime = currentTime;
        theSynopTime.SetDay(day);
        theSynopTime.SetHour(hour);
        theSynopTime.SetMin(minute);
        if (theSynopTime > currentTime)
          theSynopTime.PreviousMetTime(NFmiTimePerioid(0, 1, 0, 0, 0, 0));  // siirret‰‰n kuukausi
        // taaksep‰in, jos saatu
        // aika oli
        // tulevaisuudessa
        theSynopTime.NearestMetTime();
        return true;
      }
    }
  }
  return false;
}

static bool GetPossibleSynopCorrectionField(const std::string &str, std::string &theCorrectionField)
{
  if (str.size() == 3)
  {
    if (str[0] == str[1])  // Kahden ensimm‰isen merkin pit‰‰ olla samoja esim. RRA (tai mit‰ muita
                           // korjaus kentti tunnisteita voi olla)
    {
      if (std::isalpha(str[0]) && std::isalpha(str[2]))  // Ensimm‰isen ja viimeisen merkin pit‰‰
                                                         // olla joku kirjain (t‰llˆin myˆs 2. on
                                                         // kirjain)
      {
        theCorrectionField = str;
        return true;
      }
    }
  }
  return false;
}

// Koska synop ja muut blokit on jaettu lohkoihin AAXX, BBXX ja ZZXX sanojen mukaan ja sanoma
// tiedoston teksti jaetaan osiin niiden mukaan, k‰y seuraavaa:
// Edellisen lohkon lopussa voi olla tietoa seuraavalle lohkolle. Kaksi viimeist‰ sanaa voi olla
// seuraavan lohkon aika ja mahdollinen RRA (tai muu vastaava?) tieto.
// Siksi kun yksi lohko on k‰yty l‰pi, pit‰‰ lopusta ottaa tiedot talteen seuraavalle lohkolle.
static bool GetPossibleNextSynopBlockInfo(const std::string &synopFileBlockStr,
                                          NFmiMetTime &theSynopTime,
                                          std::string &theCorrectionField)
{
  std::pair<std::string, std::string> lastTwoWords = ::GetLastTwoWords(synopFileBlockStr);
  if (lastTwoWords.second.size() == 6)
    return ::GetPossibleSynopTime(lastTwoWords.second, theSynopTime);
  else if (lastTwoWords.first.size() == 6 && lastTwoWords.second.size() == 3)
  {
    bool status1 = ::GetPossibleSynopTime(lastTwoWords.first, theSynopTime);
    bool status2 = ::GetPossibleSynopCorrectionField(lastTwoWords.second, theCorrectionField);
    return status1 && status2;
  }
  return false;
}

void FillSynopCodeDataVectorFromSYNOPStr(std::vector<NFmiSynopCode> &theSynopCodeVector,
                                         const std::string &theSYNOPStr,
                                         NFmiAviationStationInfoSystem &theAviStations,
                                         bool fRoundTimesToNearestSynopticTimes,
                                         bool verbose,
                                         bool fDoShipMessages,
                                         bool fDoBuoyMessages,
                                         std::set<unsigned long> &theUnknownWmoIdsInOut,
                                         const std::string &theFileName)
{
  // AAXX = maa havainto ja BBXX = poiju/laiva, jotka skipataan t‰ss‰ vaiheessa
  string codeStr = fDoShipMessages ? "BBXX" : "AAXX";
  if (fDoBuoyMessages) codeStr = "ZZYY";

  std::vector<std::string> aaCodeBlocks = NFmiStringTools::Split(theSYNOPStr, codeStr);

  if (aaCodeBlocks.size() <= 1)
  {
    // Try the code string in lower case before giving up
    boost::to_lower(codeStr);

    aaCodeBlocks = NFmiStringTools::Split(theSYNOPStr, codeStr);

    // Still no result, give up
    if (aaCodeBlocks.size() <= 1) return;
  }

  NFmiMetTime possibleSynoptime = NFmiMetTime::gMissingTime;
  std::string possibleCorrectionField;
  for (int j = 0; j < static_cast<int>(aaCodeBlocks.size());
       j++)  // Huom! 1. stringi‰ ei oteta, koska se on turha headeri osa
  {
    string usedSynopBlock = aaCodeBlocks[j];
    usedSynopBlock = NFmiStringTools::TrimL(usedSynopBlock);
    bool usePossibleSynopTime = false;
    if (usedSynopBlock.size() > 1 && (usedSynopBlock[0] == '\r' || usedSynopBlock[0] == '\n'))
      usePossibleSynopTime = true;
    usedSynopBlock =
        ::TrimFromExtraSpaces(usedSynopBlock);  // pit‰‰ trimmata alku ja loppu spaceista, muuten
                                                // aika stringin sijainti laskut voivat menn‰
                                                // pieleen, kun reallyUsedSynopBlock-stringi‰
                                                // lasketaan
    const size_t kMaxErrorStrSize = 100;  // t‰m‰n verran laitetaan synop-blockin alusta cerr:iin
    try
    {
      size_t oldSynopCodeVecSize = theSynopCodeVector.size();
      ::MakeSynopCodeDataFromSYNOPStr(usedSynopBlock,
                                      theAviStations,
                                      fRoundTimesToNearestSynopticTimes,
                                      verbose,
                                      fDoShipMessages,
                                      fDoBuoyMessages,
                                      theSynopCodeVector,
                                      theUnknownWmoIdsInOut,
                                      usePossibleSynopTime,
                                      possibleSynoptime,
                                      possibleCorrectionField);
      if (oldSynopCodeVecSize == theSynopCodeVector.size())
      {
        if (::GetPossibleNextSynopBlockInfo(
                usedSynopBlock, possibleSynoptime, possibleCorrectionField))
          continue;  // otetaan talteen mahdollinen synop-aika seuraavaa blokkia varten
      }
    }
    catch (std::exception &e)
    {
      if (verbose)
      {
        std::string startOfBlock = ::GetMaxNCharsFromStart(usedSynopBlock, kMaxErrorStrSize);
        cerr << std::string(e.what()) +
                    "\nin current synop-block (showing 100 chars from start), continuing...\n" +
                    startOfBlock
             << "\nIn file: " << theFileName << endl;
      }
    }
    catch (...)
    {
      if (verbose)
      {
        std::string startOfBlock = ::GetMaxNCharsFromStart(usedSynopBlock, kMaxErrorStrSize);
        cerr << std::string(
                    "Unknown error in current synop block (showing 100 chars from start), "
                    "continuing anyway...\n") +
                    startOfBlock
             << "\nIn file: " << theFileName << endl;
      }
    }
    possibleSynoptime = NFmiMetTime::gMissingTime;
    possibleCorrectionField.clear();
  }
}

static void FillParamValues(NFmiFastQueryInfo &theInfo, FmiParameterName parId, float value)
{
  if (value != kFloatMissing)
  {
    if (theInfo.Param(parId)) theInfo.FloatValue(value);
  }
}

static void FillParamValues(NFmiFastQueryInfo &theInfo,
                            const NFmiSynopCode &theSynopCode,
                            bool fillSeaParams)
{
  ::FillParamValues(theInfo, kFmiTemperature, theSynopCode.itsTemperature);
  ::FillParamValues(theInfo, kFmiDewPoint, theSynopCode.itsDewPoint);
  ::FillParamValues(theInfo, kFmiHumidity, theSynopCode.itsRH);
  ::FillParamValues(theInfo, kFmiPressure, theSynopCode.itsPressure);
  ::FillParamValues(theInfo, kFmiWindDirection, theSynopCode.itsWD);
  ::FillParamValues(theInfo, kFmiWindSpeedMS, theSynopCode.itsWS);
  ::FillParamValues(theInfo, kFmiVisibility, theSynopCode.itsVisibility);
  ::FillParamValues(theInfo, kFmiPressureTendency, theSynopCode.itsPressureTendency);
  ::FillParamValues(theInfo, kFmiPressureChange, theSynopCode.itsPressureChange);
  ::FillParamValues(theInfo, kFmiCloudHeight, theSynopCode.itsCloudBaseHeight);
  ::FillParamValues(theInfo, kFmiTotalCloudCover, theSynopCode.itsN);
  ::FillParamValues(theInfo, kFmiPastWeather1, theSynopCode.itsW1);
  ::FillParamValues(theInfo, kFmiPastWeather2, theSynopCode.itsW2);
  ::FillParamValues(theInfo, kFmiPrecipitationAmount, theSynopCode.itsPrecipitation);

  ::FillParamValues(theInfo, kFmiPresentWeather, theSynopCode.itsPresentWeatherCode);
  ::FillParamValues(theInfo, kFmiLowCloudCover, theSynopCode.itsNh);
  ::FillParamValues(theInfo, kFmiLowCloudType, theSynopCode.itsCl);
  ::FillParamValues(theInfo, kFmiMiddleCloudType, theSynopCode.itsCm);
  ::FillParamValues(theInfo, kFmiHighCloudType, theSynopCode.itsCh);
  ::FillParamValues(theInfo, kFmiMaximumTemperature, theSynopCode.itsTmax);
  ::FillParamValues(theInfo, kFmiMinimumTemperature, theSynopCode.itsTmin);
  ::FillParamValues(theInfo, kFmiHourlyMaximumGust, theSynopCode.itsGust);

  if (fillSeaParams)
  {
    ::FillParamValues(theInfo, kFmiTemperatureSea, theSynopCode.itsTwater);
    ::FillParamValues(theInfo, kFmiLatitude, theSynopCode.itsLat);
    ::FillParamValues(theInfo, kFmiLongitude, theSynopCode.itsLon);
  }
}

static std::map<std::string, unsigned long> MakeShipNameLocationCache(NFmiFastQueryInfo &info)
{
  std::map<std::string, unsigned long> locationCache;
  for (info.ResetLocation(); info.NextLocation();)
  {
    locationCache.insert(
        std::make_pair(std::string(info.Location()->GetName()), info.LocationIndex()));
  }
  return locationCache;
}

static std::map<unsigned long, unsigned long> MakeSynopStationIdLocationCache(
    NFmiFastQueryInfo &info)
{
  std::map<unsigned long, unsigned long> locationCache;
  for (info.ResetLocation(); info.NextLocation();)
  {
    locationCache.insert(std::make_pair(info.Location()->GetIdent(), info.LocationIndex()));
  }
  return locationCache;
}

static bool SetLocationWithCache(
    NFmiFastQueryInfo &info,
    const NFmiSynopCode &theSynopCode,
    bool fDoShipMessages,
    const std::map<std::string, unsigned long> &shipNameLocationCache,
    const std::map<unsigned long, unsigned long> &synopStationIdLocationCache)
{
  unsigned long usedLocationIndex = gMissingIndex;
  if (fDoShipMessages)
    usedLocationIndex =
        shipNameLocationCache.find(std::string(theSynopCode.Station().GetName()))->second;
  else
    usedLocationIndex = synopStationIdLocationCache.find(theSynopCode.Station().GetIdent())->second;
  return info.LocationIndex(usedLocationIndex);
}

static void FillData(const std::vector<NFmiSynopCode> &theSynopCodeVector,
                     bool fDoShipMessages,
                     bool fDoBuoyMessages,
                     NFmiQueryData *theData)
{
  if (theData)
  {  // sitten t‰ytet‰‰n uusi data
    NFmiFastQueryInfo infoIter(theData);
    infoIter.FirstLevel();
    bool fillSeaParams = fDoShipMessages || fDoBuoyMessages;
    std::map<std::string, unsigned long> shipNameLocationCache;
    std::map<unsigned long, unsigned long> synopStationIdLocationCache;
    if (fDoShipMessages)
      shipNameLocationCache = ::MakeShipNameLocationCache(infoIter);
    else
      synopStationIdLocationCache = ::MakeSynopStationIdLocationCache(infoIter);

    size_t ssize = theSynopCodeVector.size();
    for (size_t k = 0; k < ssize; k++)
    {
      const NFmiSynopCode &synopCode = theSynopCodeVector[k];
      if (infoIter.Time(synopCode.Time()))
      {
        if (::SetLocationWithCache(infoIter,
                                   synopCode,
                                   fDoShipMessages,
                                   shipNameLocationCache,
                                   synopStationIdLocationCache))
        {
          ::FillParamValues(infoIter, synopCode, fillSeaParams);
        }
      }
    }
    // kun synop data on t‰ytetty, lasketaan mahd. puuttuva kosteus arvo dataan T ja Td avulla
    ::FillMissingHumidityValues(infoIter);
  }
}

NFmiQueryData *MakeQueryDataFromSynopCodeDataVector(
    const std::vector<NFmiSynopCode> &theSynopCodeVector,
    const NFmiProducer &theWantedProducer,
    bool fDoShipMessages,
    bool fDoBuoyMessages)
{
  NFmiQueryData *newData = 0;

  NFmiQueryInfo *innerInfo = MakeNewInnerInfoForSYNOP(
      theSynopCodeVector, theWantedProducer, fDoShipMessages, fDoBuoyMessages);
  std::auto_ptr<NFmiQueryInfo> innerInfoPtr(innerInfo);
  if (innerInfo)
  {
    newData = NFmiQueryDataUtil::CreateEmptyData(*innerInfo);
    ::FillData(theSynopCodeVector, fDoShipMessages, fDoBuoyMessages, newData);
  }

  return newData;
}

void Domain(int argc, const char *argv[])
{
  NFmiMilliSecondTimer timer;

  NFmiCmdLine cmdline(argc, argv, "s!p!tvSBf");

  // Tarkistetaan optioiden oikeus:

  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    ::Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  int numOfParams = cmdline.NumberofParameters();
  if (numOfParams < 1)
  {
    cerr << "Error: Atleast 1 parameter expected, 'fileFilter1 [fileFilter2 ...]'\n\n";
    ::Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  bool verbose = false;
  if (cmdline.isOption('v')) verbose = true;

  bool useWmoFlatTableFormat = false;
  if (cmdline.isOption('f')) useWmoFlatTableFormat = true;

#ifdef UNIX
  std::string stationFile = "/usr/share/smartmet/stations.csv";
#else
  std::string stationFile = "";
#endif

  NFmiAviationStationInfoSystem aviStationInfoSystem(true, verbose);

  if (cmdline.isOption('s')) stationFile = cmdline.OptionValue('s');

  if (useWmoFlatTableFormat)
    aviStationInfoSystem.InitFromWmoFlatTable(stationFile);
  else
    aviStationInfoSystem.InitFromMasterTableCsv(stationFile);

  bool doShipMessages = false;
  if (cmdline.isOption('S')) doShipMessages = true;

  bool doBuoyMessages = false;
  if (cmdline.isOption('B')) doBuoyMessages = true;

  if (doBuoyMessages && doShipMessages)
    throw runtime_error("Error with S- and B-options, don't use them at the same time.");

  NFmiProducer wantedProducer =
      doShipMessages ? NFmiProducer(kFmiSHIP, "SHIP") : NFmiProducer(kFmiSYNOP, "SYNOP");
  if (doBuoyMessages) wantedProducer = NFmiProducer(kFmiBUOY, "BUOY");
  if (cmdline.isOption('p'))
  {
    std::vector<std::string> strVector = NFmiStringTools::Split(cmdline.OptionValue('p'), ",");
    if (strVector.size() != 2)
      throw runtime_error(
          "Error: with p-option 2 comma separated parameters expected (e.g. 1001,SYNOP)");

    unsigned long prodId = NFmiStringTools::Convert<unsigned long>(strVector[0]);
    wantedProducer = NFmiProducer(prodId, strVector[1]);
  }

  bool roundTimesToNearestSynopticTimes = false;
  if (cmdline.isOption('t')) roundTimesToNearestSynopticTimes = true;

  //	1. Lue n kpl filefiltereit‰ listaan
  vector<string> fileFilterList;
  for (int i = 1; i <= numOfParams; i++)
  {
    fileFilterList.push_back(cmdline.Parameter(i));
  }

  std::set<unsigned long> unknownWmoIdsInOut;
  bool foundAnyFiles = false;
  std::vector<NFmiSynopCode> synopCodeVector;
  for (unsigned int j = 0; j < fileFilterList.size(); j++)
  {
    //	2. Hae jokaista filefilteri‰ vastaavat tiedostonimet omaan listaan
    std::string filePatternStr = fileFilterList[j];
    std::string usedPath = NFmiFileSystem::PathFromPattern(filePatternStr);
    list<string> fileList = NFmiFileSystem::PatternFiles(filePatternStr);
    for (list<string>::iterator it = fileList.begin(); it != fileList.end(); ++it)
    {
      //	3. Lue listan tiedostot vuorollaan sis‰‰n ja tulkitse siit‰ sanomat
      // synopCode-vektoriin
      std::string finalFileName = usedPath + *it;
      foundAnyFiles = true;
      string synopFileContent;
      if (NFmiFileSystem::ReadFile2String(finalFileName, synopFileContent))
      {
        string errorStr;
        ::FillSynopCodeDataVectorFromSYNOPStr(synopCodeVector,
                                              synopFileContent,
                                              aviStationInfoSystem,
                                              roundTimesToNearestSynopticTimes,
                                              verbose,
                                              doShipMessages,
                                              doBuoyMessages,
                                              unknownWmoIdsInOut,
                                              *it);
      }
      else
        cerr << "Warning, couldn't read the file: '" << finalFileName
             << "', continuing to next file..." << endl;
    }
  }
  if (foundAnyFiles == false) throw runtime_error("Error: Didn't find any files to read.");
  if (synopCodeVector.empty())
    throw runtime_error("Error: Couldn't decode any synops from any files.");
  if (verbose && unknownWmoIdsInOut.size() > 0)
  {
    cerr << "Warning, there were messages from unknown stations (" << unknownWmoIdsInOut.size()
         << ") that were ignored." << endl;
    for (std::set<unsigned long>::iterator it = unknownWmoIdsInOut.begin();
         it != unknownWmoIdsInOut.end();
         ++it)
      cerr << *it << " ";
    cerr << endl;
  }
  //	6. Tee synopCode-vektorista lopullinen data kerralla
  NFmiQueryData *data = ::MakeQueryDataFromSynopCodeDataVector(
      synopCodeVector, wantedProducer, doShipMessages, doBuoyMessages);

  //	7. talleta querydata output:iin
  if (data)
  {
    NFmiStreamQueryData sQOutData(data);
    if (!sQOutData.WriteCout())
      throw runtime_error("Error: Couldn't write combined qdata to stdout.");
    else
      cerr << "Completing the task, data was created and written to a file (stdout)." << endl;
  }
  else
    throw runtime_error("Error CombineQueryDatas: coudn't combine datas.");

  timer.StopTimer();
  std::cerr << "Elapsed time " << timer.EasyTimeDiffStr() << std::endl;
}

int main(int argc, const char *argv[])
{
  try
  {
    ::Domain(argc, argv);
  }
  catch (exception &e)
  {
    cerr << e.what() << endl;
    return 1;
  }
  catch (...)
  {
    cerr << "Unknown exception thrown in program, stopping execution..." << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
