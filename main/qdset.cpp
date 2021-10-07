/*!
 *  \file changeParam.cpp
 *  Tekijä: Marko (10.10.2002) \par
 *  Tämä ohjelma lukee stdin:in annetun qdatan, muuttaa annetun parametriId:n
 *  parametri ID:n ja nimen halutuiksi (jos ei annettu arvoa, nimi pysyy samana).
 *  Jos tuottaja ID:tä ja nimeä ei anneta, pysyvät ne ennallaan. \par
 *  Käyttö, ks. Usage \par
 *
 */

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiStringTools.h>
#include <newbase/NFmiVersion.h>

#include <macgyver/TimeParser.h>

#include <boost/filesystem/operations.hpp>

#include <fstream>
#include <stdexcept>

using namespace std;  // tätä ei saa sitten laittaa headeriin, eikä ennen includeja!!!!

void Usage();
void run(int argc, const char* argv[]);

int main(int argc, const char* argv[])
{
  try
  {
    run(argc, argv);
  }
  catch (exception& e)
  {
    cerr << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------

void Usage()
{
  cerr << "Usage: qdset [options] dataFile param(Id/name e.g. 4 or Temperature)" << endl
       << endl
       << "Options (default values are always current parameters current values):" << endl
       << endl
       << "   -n paramName\t\tNew parameter name" << endl
       << "   -d <paramId>\t\tNew parameter id (see FmiParameterName)" << endl
       << "   -N producerName\tNew producer name (changes all parameters prod name," << endl
       << "   \t\t\tno param id argument needed" << endl
       << "   -D <producerId>\tNew producer id (changes all parameters prod ID," << endl
       << "   \t\t\tno param id argument needed" << endl
       << "   -l <minValue>\tParameter's new min value" << endl
       << "   -u <maxValue>\tParameter's new max value" << endl
       << "   -i <0-5>\t\tNew interpolation method (see FmiInterpolationMethod)" << endl
       << "   -t <0-8>\t\tNew parameter type (see FmiParamType)" << endl
       << "   -s <paramScale>\tNew parameter scale value (e.g. 1)" << endl
       << "   -b <paramBase>\tNew parameter base value (e.g. 0)" << endl
       << "   -p precisionString\tNew parameter precision string (e.g. %0.1f)" << endl
       << "   -Z <value>\t\tNew level value" << endl
       << "   -L <value>\t\tNew level type" << endl
       << "   -T <time>\t\tNew UTC origin time in ISO, SQL or timestamp format" << endl
       << "   -w stationId\t\tNew station id" << endl
       << "   -W stationName\tNew station name" << endl
       << endl
       << "Example usage: qdset -n 'Temperature' dataFile Temperature" << endl
       << endl;
}

void run(int argc, const char* argv[])
{
  NFmiCmdLine cmdline(argc, argv, "n!d!N!D!l!u!i!t!s!b!p!Z!L!T!w!W!");
  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;

    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  bool producerChange = (cmdline.isOption('N') || cmdline.isOption('D'));
  bool levelChange = (cmdline.isOption('Z') || cmdline.isOption('L'));
  bool originChange = cmdline.isOption('T');

  if ((producerChange || originChange || levelChange) &&
      (cmdline.NumberofParameters() < 1 || cmdline.NumberofParameters() > 2))
  {
    cerr << "Error: atleast 1 parameter expected when changing producer, level or origin time, "
            "dataFile\n\n";
    Usage();
    throw runtime_error("");
  }
  else if (!producerChange && !levelChange && !originChange && cmdline.NumberofParameters() != 2)
  {
    cerr << "Error: 2 parameters expected, dataFile 'parameter-id/name or station id'\n\n";
    Usage();
    throw runtime_error("");  // tässä piti ensin tulostaa cerr:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  string dataFile(cmdline.Parameter(1));

  NFmiQueryData qd(dataFile, false);

  NFmiQueryInfo* info =
      qd.Info();  // tässä pitää kopeloida ihan datan sisuksia, että muutokset saadaan voimaan!

  // Katsotaan ensin onko 2. parametrina annettu parametri-tunniste nimi (esim. Temperature), vai
  // identti (esim. 4)
  bool paramFound = static_cast<bool>(cmdline.isOption('w')) || cmdline.isOption('W');
  string paramIdOrName(cmdline.Parameter(2));
  NFmiEnumConverter eConv;
  auto parNameId = static_cast<FmiParameterName>(eConv.ToEnum(paramIdOrName));
  if (parNameId != kFmiBadParameter && info->Param(parNameId))
    paramFound = true;
  if (!paramFound)
  {  // katsotaan onko id annettu ja löytyykö se
    try
    {
      parNameId = static_cast<FmiParameterName>(NFmiStringTools::Convert<int>(paramIdOrName));
      if (info->Param(parNameId))
        paramFound = true;
    }
    catch (exception& /* e */)
    {
    }
  }
  if (!paramFound)
    if (!producerChange && !originChange && !levelChange)
      throw runtime_error(string("Annettua parametria: '") + paramIdOrName +
                          "' ei löytynyt datasta.");

  NFmiDataIdent newDataIdent(info->Param());  // defaultti arvot täältä

  if (cmdline.isOption('n'))
    newDataIdent.GetParam()->SetName(cmdline.OptionValue('n'));

  if (cmdline.isOption('d'))
  {
    unsigned long id = eConv.ToEnum(cmdline.OptionValue('d'));
    if (id == kFmiBadParameter)
      id = NFmiStringTools::Convert<unsigned long>(cmdline.OptionValue('d'));
    newDataIdent.GetParam()->SetIdent(id);
  }

  if (cmdline.isOption('l'))
    newDataIdent.GetParam()->MinValue(NFmiStringTools::Convert<double>(cmdline.OptionValue('l')));

  if (cmdline.isOption('u'))
    newDataIdent.GetParam()->MaxValue(NFmiStringTools::Convert<double>(cmdline.OptionValue('u')));

  if (cmdline.isOption('i'))
    newDataIdent.GetParam()->InterpolationMethod(static_cast<FmiInterpolationMethod>(
        NFmiStringTools::Convert<int>(cmdline.OptionValue('i'))));

  if (cmdline.isOption('t'))
    newDataIdent.Type(NFmiStringTools::Convert<unsigned long>(cmdline.OptionValue('t')));

  if (cmdline.isOption('s'))
    newDataIdent.GetParam()->Scale(NFmiStringTools::Convert<float>(cmdline.OptionValue('s')));

  if (cmdline.isOption('b'))
    newDataIdent.GetParam()->Base(NFmiStringTools::Convert<float>(cmdline.OptionValue('b')));

  if (cmdline.isOption('p'))
    newDataIdent.GetParam()->Precision(cmdline.OptionValue('p'));

  info->Param() = newDataIdent;

  bool changeProducerForAllParams = false;
  NFmiProducer producer(*info->Producer());
  if (producerChange)
  {  // otetaan 1. tuottaja pohjaksi
    info->FirstParam();
    producer = *info->Producer();
  }
  if (cmdline.isOption('N'))
  {
    newDataIdent.GetProducer()->SetName(cmdline.OptionValue('N'));
    changeProducerForAllParams = true;
    producer.SetName(cmdline.OptionValue('N'));
  }
  if (cmdline.isOption('D'))
  {
    newDataIdent.GetProducer()->SetIdent(
        NFmiStringTools::Convert<unsigned long>(cmdline.OptionValue('D')));
    changeProducerForAllParams = true;
    producer.SetIdent(NFmiStringTools::Convert<unsigned long>(cmdline.OptionValue('D')));
  }

  if (changeProducerForAllParams)
    info->SetProducer(producer);

  if (cmdline.isOption('Z'))
  {
    auto newlevel = NFmiStringTools::Convert<double>(cmdline.OptionValue('Z'));
    info->First();
    info->EditLevel().LevelValue(newlevel);
  }

  if (cmdline.isOption('L'))
  {
    long newtype = NFmiStringTools::Convert<long>(cmdline.OptionValue('L'));
    info->First();
    info->EditLevel().SetIdent(newtype);
  }

  if (cmdline.isOption('T'))
  {
    std::string stamp = cmdline.OptionValue('T');
    info->OriginTime(Fmi::TimeParser::parse(stamp));
  }

  if (cmdline.isOption('w') || cmdline.isOption('W'))
  {
    if (info->IsGrid())
      throw runtime_error("Querydata is in grid format");

    long oldId;

    try
    {
      oldId = stol(paramIdOrName);
    }
    catch (const invalid_argument& e)
    {
      throw runtime_error("Station id must be all numbers");
    }

    if (!info->Location(oldId))
      throw runtime_error("station " + paramIdOrName + " not found from data");

    if (cmdline.isOption('w'))
    {
      const long newId = stol(cmdline.OptionValue('w'));

      info->EditStation().SetIdent(newId);
    }
    if (cmdline.isOption('W'))
    {
      info->EditStation().SetName(cmdline.OptionValue('W'));
    }
  }

  // Copied from NFmiStreamQueryData::WriteData for backward compatibility

  auto version = static_cast<unsigned short>(qd.InfoVersion());
  if (version < 6)
    version = 6;
  qd.InfoVersion(version);

  boost::filesystem::path p = dataFile;
  boost::filesystem::path tmp = boost::filesystem::unique_path(p.string() + "_%%%%%%%%");

  ofstream out(tmp.c_str(), ios::binary | ios::out);
  if (!out)
    throw runtime_error("Opening '" + tmp.string() + "' for writing failed");
  out << qd;
  out.close();

  boost::filesystem::remove(dataFile);
  boost::filesystem::rename(tmp, dataFile);
}
