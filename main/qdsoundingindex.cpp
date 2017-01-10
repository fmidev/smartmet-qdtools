

#include <NFmiQueryData.h>
#include <NFmiQueryDataUtil.h>
#include <NFmiCmdLine.h>
#include <NFmiMilliSecondTimer.h>
#include <NFmiSoundingIndexCalculator.h>
#include <NFmiFileString.h>
#include <NFmiSoundingFunctions.h>

#include <iostream>
#include <fstream>

static void Usage(const std::string& theExecutableName)
{
  NFmiFileString fileNameStr(theExecutableName);
  std::string usedFileName(
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimestä
  std::cerr << "Usage: " << std::endl
            << usedFileName.c_str() << " [options] qdin dqout" << std::endl
            << std::endl
            << "Options:" << std::endl
            << std::endl
            << "\t-n producer-name <default=qdin-producer-name>\tSets producer name." << std::endl
            << std::endl;
}

static void run(int argc, const char* argv[])
{
  NFmiCmdLine cmdLine(argc, argv, "n!");
  if (cmdLine.NumberofParameters() < 2)
  {
    Usage(argv[0]);
    return;
  }

  std::string fileIn = cmdLine.Parameter(1);
  std::string fileOut = cmdLine.Parameter(2);

  std::string producerName;
  if (cmdLine.isOption('n')) producerName = cmdLine.OptionValue('n');

  std::cerr << "starting the " << argv[0] << " execution" << std::endl;

  NFmiMilliSecondTimer debugTimer;
  debugTimer.StartTimer();
  boost::shared_ptr<NFmiQueryData> data =
      NFmiSoundingIndexCalculator::CreateNewSoundingIndexData(fileIn, producerName, true, 0, false);
  debugTimer.StopTimer();

  std::string debugStr("Making ");
  debugStr += fileOut;
  debugStr += " data lasted ";
  debugStr += debugTimer.EasyTimeDiffStr();
  std::cerr << debugStr << std::endl;

  data->Write(fileOut);
  std::cerr << "stored data to file: " << fileOut << std::endl;
}

int main(int argc, const char* argv[])
{
  try
  {
    run(argc, argv);
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
