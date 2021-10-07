

#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiFileString.h>
#include <newbase/NFmiMilliSecondTimer.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <smarttools/NFmiSoundingFunctions.h>
#include <smarttools/NFmiSoundingIndexCalculator.h>

#include <fstream>
#include <iostream>

static void Usage(const std::string& theExecutableName)
{
  NFmiFileString fileNameStr(theExecutableName);
  std::string usedFileName(
      fileNameStr.FileName().CharPtr());  // ota pois mahd. polku executablen nimestä
  std::cerr
      << "Usage: " << std::endl
      << usedFileName.c_str() << " [options] qdin dqout" << std::endl
      << std::endl
      << "Options:" << std::endl
      << std::endl
      << "\t-n producer-name <default=qdin-producer-name>\tSets producer name." << std::endl
      << "\t-t thread count <default=all>\tHow many worker threads will be doing the calculations."
      << std::endl
      << std::endl;
}

static void run(int argc, const char* argv[])
{
  NFmiCmdLine cmdLine(argc, argv, "n!t!");
  if (cmdLine.NumberofParameters() < 2)
  {
    Usage(argv[0]);
    return;
  }

  std::string fileIn = cmdLine.Parameter(1);
  std::string fileOut = cmdLine.Parameter(2);

  std::string producerName;
  if (cmdLine.isOption('n'))
    producerName = cmdLine.OptionValue('n');

  // Default 0 uses all available CPU cores
  int workerThreadCount = 0;
  if (cmdLine.isOption('t'))
    workerThreadCount = std::stoi(cmdLine.OptionValue('t'));

  std::cerr << "starting the " << argv[0] << " execution" << std::endl;

  NFmiMilliSecondTimer debugTimer;
  debugTimer.StartTimer();
  boost::shared_ptr<NFmiQueryData> data = NFmiSoundingIndexCalculator::CreateNewSoundingIndexData(
      fileIn, producerName, true, nullptr, false, workerThreadCount);
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
