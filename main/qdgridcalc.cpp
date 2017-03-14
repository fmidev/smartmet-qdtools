
// qdgridcalc.cpp  (oli ennen GetSuitableGrid.cpp)

#ifndef UNIX
//#pragma warning(disable : 4511 4512 4100 4127) // Remove boost warnings from VC++
#endif

#include <newbase/NFmiArea.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiLocation.h>
#include <newbase/NFmiQueryInfo.h>
#include <newbase/NFmiStringTools.h>

#include <boost/shared_ptr.hpp>
#include <fstream>

using namespace std;

void Domain(int argc, const char *argv[]);

// ----------------------------------------------------------------------
// Kaytto-ohjeet
// ----------------------------------------------------------------------
void Usage(void)
{
  cout << "Usage: qdgridcalc [options] projectionstring x_km y_km" << endl
       << endl
       << "Program gives you grid x- and y-sizes for given projection" << endl
       << "and given x- and y-direction wanted resolutions (km)." << endl
       << endl
       << "\t-s \tWanted grid sizes are given as arguments already, just" << endl
       << "\t\tcalculate grid resolution." << endl
       << "\t-f qddatafile \tWith this option you get to know what is the grid sizes" << endl
       << "\t\tof the given qd-data. This overrides the need to give other" << endl
       << "\t\tother arguments (projectionstring x_km y_km)" << endl
       << endl;
}

int main(int argc, const char *argv[])
{
  try
  {
    ::Domain(argc, argv);
  }
  catch (exception &e)
  {
    cout << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

static void PrintGridSizeInfo(const NFmiArea *area,
                              unsigned long closestXGridSize,
                              unsigned long closestYGridSize,
                              bool fActualGridUsed)
{
  NFmiGrid idealGrid(area, closestXGridSize, closestYGridSize);
  // 1. ala keski x-dist
  NFmiLocation loc1(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, 0)));
  NFmiLocation loc2(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2 + 1, 0)));
  double alaMidX = loc1.Distance(loc2) / 1000.;
  // 2. ala keski y-dist
  loc1 = NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, 0)));
  loc2 = NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, 1)));
  double alaMidY = loc1.Distance(loc2) / 1000.;

  // 3. keski keski x-dist
  loc1 =
      NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize / 2)));
  loc2 = NFmiLocation(
      idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2 + 1, closestYGridSize / 2)));
  double keskiMidX = loc1.Distance(loc2) / 1000.;
  // 4. ala keski y-dist
  loc1 =
      NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize / 2)));
  loc2 = NFmiLocation(
      idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize / 2 + 1)));
  double keskiMidY = loc1.Distance(loc2) / 1000.;

  // 5. yl‰ keski x-dist
  loc1 =
      NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize - 1)));
  loc2 = NFmiLocation(
      idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2 + 1, closestYGridSize - 1)));
  double ylaMidX = loc1.Distance(loc2) / 1000.;
  // 6. yl‰ keski y-dist
  loc1 =
      NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize - 2)));
  loc2 =
      NFmiLocation(idealGrid.GridToLatLon(NFmiPoint(closestXGridSize / 2, closestYGridSize - 1)));
  double ylaMidY = loc1.Distance(loc2) / 1000.;

  string verbiStr("would be");
  if (fActualGridUsed) verbiStr = "is";
  if (fActualGridUsed) cout << "Given querydata projection: " << area->AreaStr() << endl;
  cout << "Grid size " << verbiStr << ": " << closestXGridSize << " x " << closestYGridSize << endl;
  cout << "Grid size " << verbiStr << " at center of bottom edge: " << alaMidX << " x " << alaMidY
       << " km" << endl;
  cout << "Grid size " << verbiStr << " at center:           " << keskiMidX << " x " << keskiMidY
       << " km" << endl;
  cout << "Grid size " << verbiStr << " at center of top edge: " << ylaMidX << " x " << ylaMidY
       << " km" << endl;
}

static void DoQdDataGridSizePrint(const string &theFileName)
{
  ifstream in(theFileName.c_str(), ios::binary);
  if (in)
  {
    NFmiQueryInfo info;
    in >> info;  // luetaan qdatasta vain info osuus (nopeampaa)

    if (info.Grid())
    {
      string projStr = info.Area()->AreaStr();
      ::PrintGridSizeInfo(info.Area(), info.Grid()->XNumber(), info.Grid()->YNumber(), true);
    }
    else
      throw runtime_error("Given querydata is not grid data.");
  }
  else
    throw runtime_error(string("Cannot open qd-file: ") + theFileName);
}

void Domain(int argc, const char *argv[])
{
  NFmiCmdLine cmdline(argc, argv, "sf!");

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cout << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    ::Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cout:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  bool doDataGridSizeCheck = false;
  string dataFileName;
  if (cmdline.isOption('f'))
  {
    doDataGridSizeCheck = true;
    dataFileName = cmdline.OptionValue('f');
  }

  int numOfParams = cmdline.NumberofParameters();
  if (doDataGridSizeCheck == false && numOfParams != 3)
  {
    cout << "Error: Atleast 3 parameter expected, 'projectionstring x_km y_km'\n\n";
    ::Usage();
    throw runtime_error("");  // t‰ss‰ piti ensin tulostaa cout:iin tavaraa ja sitten vasta Usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (doDataGridSizeCheck)
  {
    ::DoQdDataGridSizePrint(dataFileName);
    return;
  }

  bool useGridSizes = false;
  if (cmdline.isOption('s')) useGridSizes = true;

  std::string projStr = cmdline.Parameter(1);
  std::string xKmStr = cmdline.Parameter(2);
  double xRes = NFmiStringTools::Convert<double>(xKmStr);
  std::string yKmStr = cmdline.Parameter(3);
  double yRes = NFmiStringTools::Convert<double>(yKmStr);

  boost::shared_ptr<NFmiArea> areaPtr = NFmiAreaFactory::Create(projStr);
  if (areaPtr.get())
  {
    if (useGridSizes)
      ::PrintGridSizeInfo(areaPtr.get(),
                          static_cast<unsigned long>(xRes),
                          static_cast<unsigned long>(yRes),
                          doDataGridSizeCheck);
    else
    {
      unsigned long testGridSize = 50;
      unsigned long i = 0;
      double minDist = 999999999;
      double lastDist = minDist;
      unsigned long closestXGridSize = 2;
      // etsit‰‰n ensin sopiva x-ulottuvuus
      for (i = 2; i < 5000; i++)
      {
        NFmiGrid grid(areaPtr.get(), i, testGridSize);
        NFmiLocation loc1(grid.GridToLatLon(NFmiPoint(i / 2, testGridSize / 2)));
        NFmiLocation loc2(grid.GridToLatLon(NFmiPoint(i / 2 + 1, testGridSize / 2)));
        double currDist = ::fabs(loc1.Distance(loc2) / 1000. - xRes);
        if (currDist < minDist)
        {
          minDist = currDist;
          closestXGridSize = i;
        }
        if (lastDist < currDist) break;  // jos ero haluttuun alkaa taas kasvamaan, lopetetaan
        lastDist = currDist;
      }

      // etsit‰‰n sitten sopiva y-ulottuvuus
      minDist = 999999999;
      lastDist = minDist;
      unsigned long closestYGridSize = 2;
      for (i = 2; i < 5000; i++)
      {
        NFmiGrid grid(areaPtr.get(), testGridSize, i);
        NFmiLocation loc1(grid.GridToLatLon(NFmiPoint(testGridSize / 2, i / 2)));
        NFmiLocation loc2(grid.GridToLatLon(NFmiPoint(testGridSize / 2, i / 2 + 1)));
        double currDist = ::fabs(loc1.Distance(loc2) / 1000. - yRes);
        if (currDist < minDist)
        {
          minDist = currDist;
          closestYGridSize = i;
        }
        if (lastDist < currDist) break;  // jos ero haluttuun alkaa taas kasvamaan, lopetetaan
        lastDist = currDist;
      }
      ::PrintGridSizeInfo(areaPtr.get(), closestXGridSize, closestYGridSize, doDataGridSizeCheck);
    }
  }
}
