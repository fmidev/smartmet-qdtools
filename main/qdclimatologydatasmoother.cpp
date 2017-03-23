#include <NFmiQueryData.h>
#include <NFmiCmdLine.h>
#include <NFmiFastQueryInfo.h>
#include <NFmiStreamQueryData.h>
#include <NFmiFileSystem.h>
#include <NFmiQueryDataUtil.h>
#include <NFmiTotalWind.h>
#include <NFmiTimeList.h>
#include <NFmiDataModifierAvg.h>
#include <NFmiTimeDescriptor.h>

#include <fstream>

using namespace std;

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------
void Usage(void)
{
    cout << "Usage: qdclimatologydatasmoother [options] qdin qdout" << endl;
    cout << "qdin must have annual climatological data. It can be station or grid data." << endl;
    cout << "Smoothing is calculated by calculating average over given days to smooth." << endl;
    cout << "If day value is 5, average is calculated from 2 days preor and 2 days after." << endl;
    cout << "the calculation time. Values are taken from those days only from the same utc hour." << endl;
    cout << "-d days-smoothed <default 3>\tOver how many days is smoothing calculated." << endl;
    cout << "-p 240,Ecmwf\tSet result data's producer id and name as wanted." << endl;
    cout << "\tdays-smoothed must be positive odd integer." << endl;
}

static void MakeErrorAndUsageMessages(const string &errorMessage)
{
    cout << errorMessage << endl;
    ::Usage();
    // tässä piti ensin tulostaa cout:iin tavaraa ja sitten vasta Usage,
    // joten en voinut laittaa virheviesti poikkeuksen mukana.
    throw runtime_error("");
}

static void MakeCheckDataError(const string &filePath, const string &baseErrorString)
{
    string errorString = baseErrorString;
    errorString += "\nFile was: ";
    errorString += filePath;
    ::MakeErrorAndUsageMessages(errorString);
}

// No checks yet
static void CheckData(NFmiFastQueryInfo &info, const string &filePath)
{
    auto &timeDescriptor = info.TimeDescriptor();
    auto diffInDays = timeDescriptor.LastTime().DifferenceInDays(timeDescriptor.FirstTime());
    if(diffInDays < 364 || diffInDays > 365)
        MakeCheckDataError(filePath, "Error: input data doesn't contains time spread suitable for one year climatplogical data");
}


static unique_ptr<NFmiQueryData> CreateNewEmtyData(NFmiQueryInfo &info)
{
    return unique_ptr<NFmiQueryData>(NFmiQueryDataUtil::CreateEmptyData(info));
}

float CalcSmoothedValue(NFmiFastQueryInfo &sourceInfo, const std::vector<unsigned long> &timeIndices)
{
    NFmiDataModifierAvg avg;
    for(auto timeIndex : timeIndices)
    {
        sourceInfo.TimeIndex(timeIndex);
        avg.Calculate(sourceInfo.FloatValue());
    }
    return avg.CalculationResult();
}

static std::vector<unsigned long> MakeTimeIndexVector(NFmiFastQueryInfo &info, int smoothedDays)
{
    auto originalTimeIndex = info.TimeIndex();
    std::vector<unsigned long> calculatedTimeIndices;
    auto backwardDaysCount = (smoothedDays - 1) / 2;
    NFmiMetTime wantedTime = info.Time();
    wantedTime.ChangeByDays(-backwardDaysCount);
    for(int i = 0; i < smoothedDays; i++, wantedTime.ChangeByDays(1))
    {
        if(info.Time(wantedTime))
            calculatedTimeIndices.push_back(info.TimeIndex());
        else
        {
            // Lets try both ends of the year to find wanted time
            // Remember that we have year long climatology data, and we have to look data over the ends from the other side of data.
            NFmiMetTime previousYearTime(wantedTime);
            previousYearTime.SetYear(previousYearTime.GetYear() - 1);
            if(info.Time(previousYearTime))
                calculatedTimeIndices.push_back(info.TimeIndex());
            else
            {
                NFmiMetTime nextYearTime(wantedTime);
                nextYearTime.SetYear(nextYearTime.GetYear() + 1);
                if(info.Time(nextYearTime))
                    calculatedTimeIndices.push_back(info.TimeIndex());
                else
                {
                    std::cout << "\nNon found time with time-index " << originalTimeIndex << " and time " << wantedTime << ", error in data?" << std::endl;
//                    ::MakeErrorAndUsageMessages("Error: It seems that given data doesn't have the right year long data set, stopping execution...");
                }
            }
        }
    }

    // Must restore original timeIndex at end
    info.TimeIndex(originalTimeIndex);
    return calculatedTimeIndices;
}

static void FillOneTimeStep(NFmiFastQueryInfo &destinationInfo, NFmiFastQueryInfo &sourceInfo, const std::vector<unsigned long> &timeIndices)
{
    for(destinationInfo.ResetParam(), sourceInfo.ResetParam(); destinationInfo.NextParam() && sourceInfo.NextParam(); )
    {
        for(destinationInfo.ResetLevel(), sourceInfo.ResetLevel(); destinationInfo.NextLevel() && sourceInfo.NextLevel(); )
        {
            for(destinationInfo.ResetLocation(), sourceInfo.ResetLocation(); destinationInfo.NextLocation() && sourceInfo.NextLocation(); )
            {
                destinationInfo.FloatValue(::CalcSmoothedValue(sourceInfo, timeIndices));
            }
        }
    }
}

static void FillNewData(const unique_ptr<NFmiQueryData> &newData, NFmiFastQueryInfo &sourceInfo, int smoothedDays)
{
    NFmiFastQueryInfo destInfo(newData.get());
    sourceInfo.First();
    // Time must be the out most loop, because we have to calculate for each time its own timeIndex list 
    // for those times that will be used for smoothing calculations.
    for(destInfo.ResetTime(); destInfo.NextTime(); )
    {
        auto timeIndex = destInfo.TimeIndex();
        std::cout << timeIndex << " ";
        ::FillOneTimeStep(destInfo, sourceInfo, ::MakeTimeIndexVector(destInfo, smoothedDays));
    }
}

static NFmiTimePerioid CalcTimeListResolution(NFmiTimeList &timeList)
{
    // Check for most common time difference from the first 20 times in time list and return it.
    // map contains each different time-step in minutes and their count.
    map<int, size_t> timeDiffInMinutesCounter;
    size_t i = 0;
    for(timeList.Reset(); timeList.Next() && i < 20; i++)
    {
        timeDiffInMinutesCounter[timeList.CurrentResolution()]++;
    }

    // Can't use C++14 yet so can't use auto with lambda parameters
    using pair_type = decltype(timeDiffInMinutesCounter)::value_type;
    auto maxElement = max_element(
        timeDiffInMinutesCounter.begin(), timeDiffInMinutesCounter.end(), 
        [&](const pair_type &element1, const pair_type &element2) 
    {return element1.second < element2.second; }
    );

    return NFmiTimePerioid(maxElement->first);
}

static NFmiTimeBag MakeNewValidTimeBag(NFmiTimeList &timeList)
{
    NFmiTimePerioid resolution = ::CalcTimeListResolution(timeList);
    return NFmiTimeBag(timeList.FirstTime(), timeList.LastTime(), resolution);
}

static NFmiQueryInfo MakeNewMetaInfo(NFmiFastQueryInfo &info)
{
    // If original data has timeList type timeDescriptor, new metaInfo
    // will contain timeBag type time structure, because its faster to use
    // in SmartMet's timeSerialViews.
    if(info.TimeDescriptor().ValidTimeBag())
        return NFmiQueryInfo(info);
    else
    {
        NFmiTimeBag validTimes = ::MakeNewValidTimeBag(*(info.TimeDescriptor().ValidTimeList()));
        NFmiTimeDescriptor times(info.OriginTime(), validTimes);
        return NFmiQueryInfo(info.ParamDescriptor(), times, info.HPlaceDescriptor(), info.VPlaceDescriptor());
    }
}

static unique_ptr<NFmiQueryData> CreateFilteredData(NFmiFastQueryInfo &info, int smoothedDays, const NFmiProducer &wantedProducer)
{
    unique_ptr<NFmiQueryData> newData = ::CreateNewEmtyData(::MakeNewMetaInfo(info));
    if(newData && wantedProducer.GetIdent())
        newData->Info()->SetProducer(wantedProducer);
    ::FillNewData(newData, info, smoothedDays);
    return newData;
}

static void WriteQueryData(unique_ptr<NFmiQueryData> &data, const string &filePath)
{
    if(!data)
        ::MakeErrorAndUsageMessages("Error: unable to create filtered data.");
    else
    {
        if(!NFmiFileSystem::CreateDirectory(NFmiFileSystem::PathFromPattern(filePath)))
        {
            string errorMessage = "Error: unable to create target directory in:\n";
            errorMessage += NFmiFileSystem::PathFromPattern(filePath);
            ::MakeErrorAndUsageMessages(errorMessage);
        }
        NFmiStreamQueryData sqData;
        if(!sqData.WriteData(filePath, data.get(), static_cast<long>(data->InfoVersion())))
        {
            string errorMessage = "Error: unable to write data to file:\n";
            errorMessage += filePath;
            ::MakeErrorAndUsageMessages(errorMessage);
        }
    }
}

static int GetSmoothedDays(NFmiCmdLine &cmdLine)
{
    int smoothedDays = 3;
    if(cmdLine.isOption('d'))
    {
        smoothedDays = boost::lexical_cast<int>(cmdLine.OptionValue('d'));
        if(smoothedDays < 3)
            ::MakeErrorAndUsageMessages("Error: Invalid smoothed-days value given, must be positive odd number and at least 3");
        if(smoothedDays % 2 == 0)
            ::MakeErrorAndUsageMessages("Error: Invalid smoothed-days value given, must be odd number");
        if(smoothedDays > 50)
            ::MakeErrorAndUsageMessages("Error: Invalid smoothed-days value given, can't be over 50 days, that would give questianable results");
    }
    return smoothedDays;
}

static NFmiProducer GetProducer(NFmiCmdLine &cmdLine)
{
    // Default producer with 0 id is not used at all.
    NFmiProducer producer(0, "");
    if(cmdLine.isOption('p'))
    {
        std::vector<std::string> strVector = NFmiStringTools::Split(cmdLine.OptionValue('p'), ",");
        if(strVector.size() != 2)
            ::MakeErrorAndUsageMessages("Error: Error: with -p option, 2 comma separated parameters expected (e.g. 240,Ec)");
        producer.SetIdent(boost::lexical_cast<unsigned long>(strVector[0]));
        producer.SetName(strVector[1]);
    }
    return producer;
}

void Domain(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "d!p!");

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
      string errorMessage("Error: Invalid command line:\n");
      errorMessage += cmdline.Status().ErrorLog();
      ::MakeErrorAndUsageMessages(errorMessage);
  }
  if(cmdline.NumberofParameters() != 2)
  {
      ::MakeErrorAndUsageMessages("Error: Invalid command line argument count, must have 2 parameters (qdin qdout)");
  }
  string fileNameIn = cmdline.Parameter(1);
  string fileNameOut = cmdline.Parameter(2);
  int smoothedDays = ::GetSmoothedDays(cmdline);
  NFmiProducer wantedProducer = ::GetProducer(cmdline);

  NFmiQueryData qData(fileNameIn);
  NFmiFastQueryInfo info(&qData);
  ::CheckData(info, fileNameIn);
  unique_ptr<NFmiQueryData> newData = CreateFilteredData(info, smoothedDays, wantedProducer);
  ::WriteQueryData(newData, fileNameOut);
}

int main(int argc, const char *argv[])
{
    try
    {
        ::Domain(argc, argv);
    }
    catch(exception &e)
    {
        cout << e.what() << endl;
        return 1;
    }
    return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
