/*!
 *  \file
 *
 *  Tekij�t: Tuikku ja Marko (29.1.2001)
 *
 *  T�m� ohjelma luo uuden datan, johon interpoloidaan olemassaolevasta datasta
 *  arvot halutulle aikaresoluutiolle mahdollisimman turvallisesti ja oikein.
 *  Voit my�s antaa alkuajan py�ristys arvon (minuuteissa) jos haluat.
 *
 *  Muutos 13.6.2002/Marko Lis�sin uuden argumentin, jolla voi s��t��
 *  aikainterpolointia. Oletus arvo on 6 tuntia eli 360 minuuttia.
 *  T�m� tarkoittaa, ett� jos interpoloitaessa ei l�ydy mit��n arvoa
 *  6 tunnin sis�lt�, tulee arvoksi puuttuvaa. Jos halutaan ettei
 *  ole mit��n rajoituksia, kun interpoloidaan ajassa, annetaan rajaksi 0.
 *  Tekee oletuksena yleisesti lineaarisen-interpoloinnin.
 *  Halutessa voidaan laittaa tekem��n lagrange interpolointia antamalla
 *  5. argumenttina 5 (ks. nurero eri menetelmille FmiGlobals.h ja
 *  FmiInterpolationMethod-enum).
 *
 *  Jos parametri on yhdistelm�parametri tai sen parametrin interpolointi
 *  metodiksi on asetettu nearestPoint, ei lagrangea tehd� kyseisille
 *  parametreille.
 */

#include <boost/lexical_cast.hpp>
#include <newbase/NFmiCmdLine.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
//#include "NFmiMilliSecondTimer.h"

using namespace std;  // t�t� ei saa sitten laittaa headeriin, eik� ennen includeja!!!!

void usage()
{
  cout << "Usage: qdinterpolatetime [options] timeResolutionInMinutes "
          "[startTimeResolutionInMinutes] [maxSearchRangeInMinutes] < infile > outfile"
       << endl
       << endl
       << "Options:" << endl
       << "  -i infile\tRead given inputfile instead of stdin" << endl
       << "  -o outfile\tWrite to given filename instead of stdout" << endl
       << endl
       << "Default value for inputfile and outputfile is '-' indicating stdin/stdout" << endl
       << endl;
}

int run(int argc, const char* argv[])
{
  NFmiCmdLine cmdline(argc, argv, "i!o!");

  string inputfile = "-";
  string outputfile = "-";

  // Tarkistetaan optioiden oikeus:
  if (cmdline.Status().IsError())
  {
    cerr << "Error: Invalid command line:" << endl << cmdline.Status().ErrorLog().CharPtr() << endl;
    usage();
    throw runtime_error("");  // t�ss� piti ensin tulostaa cerr:iin tavaraa ja sitten vasta usage,
                              // joten en voinut laittaa virheviesti poikkeuksen mukana.
  }

  if (cmdline.isOption('i')) inputfile = cmdline.OptionValue('i');

  if (cmdline.isOption('o')) outputfile = cmdline.OptionValue('o');

  int numpar = cmdline.NumberofParameters();

  if (numpar < 1 || numpar > 4)
  {
    usage();
    throw std::runtime_error("Invalid number of command line arguments");
  }

  int startTimeResolutionInMinutes = static_cast<int>(kFloatMissing);
  int maxSearchRangeInMinutes = 360;
  FmiInterpolationMethod generalInterpolationMethod = kLinearly;

  int timeResolutionInMinutes = boost::lexical_cast<int>(cmdline.Parameter(1));

  if (numpar >= 2) startTimeResolutionInMinutes = boost::lexical_cast<int>(cmdline.Parameter(2));
  if (numpar >= 3) maxSearchRangeInMinutes = boost::lexical_cast<int>(cmdline.Parameter(3));
  if (numpar >= 4)
  {
    int interp = boost::lexical_cast<int>(cmdline.Parameter(4));
    generalInterpolationMethod = FmiInterpolationMethod(interp);
  }

  NFmiQueryData qd(inputfile);

  // t�h�n toiminnot

  NFmiQueryData* newData = NFmiQueryDataUtil::InterpolateTimes(&qd,
                                                               timeResolutionInMinutes,
                                                               startTimeResolutionInMinutes,
                                                               0,
                                                               maxSearchRangeInMinutes,
                                                               generalInterpolationMethod);

  if (outputfile == "-")
    newData->Write();
  else
    newData->Write(outputfile);

  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}

int main(int argc, const char* argv[])
{
  try
  {
    run(argc, argv);
  }
  catch (exception& e)
  {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }
  return 0;  // homma meni putkeen palauta 0 onnistumisen merkiksi!!!!
}
