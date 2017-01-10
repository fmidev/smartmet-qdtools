
// nc2qd : netCDF -> queryData -filter

#include <iostream>
#include <string>
#include <vector>
#include "FmiNetCdfQueryData.h"
#include "NFmiCmdLine.h"
#include "NFmiFileString.h"
#include "NFmiMilliSecondTimer.h"
#include "NFmiProducer.h"
#include "NFmiStreamQueryData.h"
#include "NFmiStringTools.h"

static void Usage(const std::string &theExecutableName)
{
  NFmiFileString fileNameStr(theExecutableName);
  std::string usedFileName(
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimestä
  std::cerr << "Usage: " << std::endl
            << usedFileName.c_str() << " [options] nc_in qd_out" << std::endl
            << std::endl
            << "Options:" << std::endl
            << std::endl
            << "\t-p producer <default=1201,ncprod>\tSets producer id and name." << std::endl
            << std::endl;
}

static NFmiProducer GetProducer(const std::string &theProdStr)
{
  std::vector<std::string> prodParts = NFmiStringTools::Split(theProdStr);
  if (prodParts.size() == 2)
  {
    NFmiProducer prod(NFmiStringTools::Convert<unsigned long>(prodParts[0]), prodParts[1]);
    return prod;
  }
  throw std::runtime_error(
      "Error in GetProducer - check your -p option (e.g. \"-p 123,MyProducer\")");
}

static void Domain(int argc, const char *argv[])
{
  NFmiCmdLine cmdLine(argc, argv, "p!");
  if (cmdLine.NumberofParameters() < 2)
  {
    Usage(argv[0]);
    return;
  }

  std::string ncFileIn = cmdLine.Parameter(1);
  std::string qdFileOut = cmdLine.Parameter(2);

  NFmiProducer producer(1201, "ncprod");
  if (cmdLine.isOption('p')) producer = ::GetProducer(cmdLine.OptionValue('p'));

  std::cerr << "starting the " << argv[0] << " execution" << std::endl;

  NFmiMilliSecondTimer debugTimer;
  debugTimer.StartTimer();
  FmiNetCdfQueryData nc2QdFilter;
  nc2QdFilter.Producer(producer);
  std::vector<NFmiQueryData *> qDatas = nc2QdFilter.CreateQueryDatas(ncFileIn);
  if (qDatas.size() == 0) throw std::runtime_error(nc2QdFilter.ErrorMessage());
  debugTimer.StopTimer();

  std::string debugStr("Making conversion from ");
  debugStr += ncFileIn;
  debugStr += " -ncdata lasted ";
  debugStr += debugTimer.EasyTimeDiffStr();
  std::cerr << debugStr << std::endl;

  NFmiStreamQueryData sQueryData;
  for (size_t i = 0; i < qDatas.size(); i++)
  {
    if (qDatas[i])
    {
      NFmiFileString usedFileName(qdFileOut);
      qDatas[i]->Info()->FirstLevel();
      FmiLevelType levelType = qDatas[i]->Info()->Level()->LevelType();
      if (levelType == kFmiHybridLevel)
        usedFileName.Header(usedFileName.Header() + "_hyb");
      else if (levelType == kFmiHeight)
        usedFileName.Header(usedFileName.Header() + "_hgt");
      else if (levelType == kFmiPressureLevel)
        usedFileName.Header(usedFileName.Header() + "_pre");
      else if (qDatas[i]->Info()->SizeLevels() == 1)
        usedFileName.Header(usedFileName.Header() + "_sfc");
      else
        usedFileName.Header(usedFileName.Header() + "_xxx");
      if (sQueryData.WriteData(
              usedFileName, qDatas[i], static_cast<long>(qDatas[i]->InfoVersion())))
        std::cerr << "stored data to file: " << usedFileName.CharPtr() << std::endl;
      else
        throw std::runtime_error(std::string("ERROR when trying to store data to file: ") +
                                 qdFileOut);
    }
  }
}

int main(int argc, const char *argv[])
{
  try
  {
    Domain(argc, argv);
  }
  catch (std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
