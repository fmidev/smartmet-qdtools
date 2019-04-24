#include "GeoTiffQD.h"
#include <boost/shared_ptr.hpp>
#include <gdal/gdal_priv.h>
#include <gdal/ogr_spatialref.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiAreaFactory.h>
#include <newbase/NFmiQueryData.h>
#include <iomanip>
#include <iostream>
#include <stdio.h>

// float * fillFloatRasterByQD(NFmiFastQueryInfo * theData, int width, int height);
// int * fillIntRasterByQD(NFmiFastQueryInfo * theData, int width, int height);
// NFmiArea * CreteEpsgArea(string epsgCode);

void GeoTiffQD::SetTestMode(bool testMode) { isDrawGridLines = testMode; }
GeomDefinedType GeoTiffQD::ConverQD2GeoTiff(string aNameVersion,
                                            NFmiFastQueryInfo *theData,
                                            NFmiFastQueryInfo *theExternal,
                                            string tsrs,
                                            bool selectedDataType,
                                            double scale)
{
  isIntDataType = selectedDataType;
  itsScale = scale;

  GeomDefinedType geomDefinedType = kUndefinedGeom;

  const NFmiArea *area = theData->HPlaceDescriptor().Area();
  auto id = area->Proj().DetectClassId();

  if (id == kNFmiLatLonArea)
    ConvertToGeoTiff(aNameVersion, theData, theExternal, geomDefinedType = kLatLonGeom);
  else if (id == kNFmiRotatedLatLonArea)
    ConvertToGeoTiff(aNameVersion, theData, theExternal, geomDefinedType = kYkjGeom);
  else if (id == kNFmiStereographicArea)
  {
    ConvertToGeoTiff(aNameVersion, theData, theExternal, geomDefinedType = kStereoGeom);
    if (area->Proj().GetDouble("lat_0") == 10.0) geomDefinedType = kStereoGeom10;
    if (area->Proj().GetDouble("lat_0") == 20.0) geomDefinedType = kStereoGeom20;
  }
  else if (id == kNFmiRotatedLatLonArea)
    ConvertToGeoTiff(aNameVersion, theData, theExternal, geomDefinedType = kRotatedGeom);
  else
    printf("\n%s\n", "Not supported Projection");

  return geomDefinedType;
}

double GeoTiffQD::calculateTrueNorthAzimuthValue(float value,
                                                 const NFmiArea *area,
                                                 const NFmiPoint *point)
{
  double retValue = value;

  if (area != 0)
  {
    double realNorth = area->TrueNorthAzimuth(*point).Value();
    if (realNorth > 180)
    {
      realNorth = -(360 - realNorth);
    }

    if (false)
    {
      retValue = realNorth;
    }
    else
    {
      retValue = value + realNorth;
      if (retValue > 360) retValue = retValue - 360;
      if (retValue < 0) retValue = 360 + retValue;
    }
  }

  return (int)retValue;
}

void drawGridLines(const NFmiPoint &latLon, double &value)
{
  for (int i = 55; i <= 65; i += 5)
  {
    if (latLon.Y() > (i - 0.1) && latLon.Y() < (i + 0.1)) value = 0;
  }

  for (int k = -90; k <= 90; k += 5)
  {
    if (latLon.X() > (k - 0.1) && latLon.X() < (k + 0.1)) value = 0;
  }
}

void GeoTiffQD::ConvertToGeoTiff(string aNameVersion,
                                 NFmiFastQueryInfo *orginData,
                                 NFmiFastQueryInfo *theExternal,
                                 GeomDefinedType geomDefinedType)
{
  const char *pszFormat = "gtiff";
  GDALDriver *poDriver;
  // char **papszMetadata = nullptr;
  NFmiFastQueryInfo *theData = orginData;

  // bool makeInt32 = true;  //false=Float32, true=Int32

  printf("Converting .. %s\n", aNameVersion.c_str());

  // Create()
  GDALDataset *poDstDS;

  OGRSpatialReference _qdpr_srs;

  // QD-information
  const NFmiArea *area = theData->HPlaceDescriptor().Area();
  const NFmiGrid *grid = theData->HPlaceDescriptor().Grid();
  int width = grid->XNumber();
  int height = grid->YNumber();

  double tlLon = area->TopLeftLatLon().X();
  double tlLat = area->TopLeftLatLon().Y();

  // double blLon = area->BottomLeftLatLon().X();
  // double blLat = area->BottomLeftLatLon().Y();

  double brLon = area->BottomRightLatLon().X();
  double brLat = area->BottomRightLatLon().Y();

  double aLon = (brLon - tlLon) / width;
  double aLat = (tlLat - brLat) / height;

  OGRSpatialReference oSRS;
  // char *pszSRS_WKT = nullptr;

  // Data
  GDALRasterBand *poBand;

  // Register
  GDALAllRegister();

  poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);

  if (poDriver == nullptr) exit(1);

  // papszMetadata = poDriver->GetMetadata();

  /*
       if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATE, FALSE ) )
       printf( "Driver %s supports Create() method.\n", pszFormat );
   if( CSLFetchBoolean( papszMetadata, GDAL_DCAP_CREATECOPY, FALSE ) )
       printf( "Driver %s supports CreateCopy() method.\n", pszFormat );
       */

  int paramSize = 1;  // theData->ParamBag().GetSize();
  if (theExternal != 0) paramSize = 2;

  char **papszOptions = nullptr;
  // papszOptions = CSLSetNameValue( papszOptions, "COMPRESS", "PACKBITS" );

  // GDT_Int32 -or- GDT_Float32  1/4
  if (isIntDataType)
    poDstDS =
        poDriver->Create(aNameVersion.c_str(), width, height, paramSize, GDT_Int32, papszOptions);
  else
    poDstDS =
        poDriver->Create(aNameVersion.c_str(), width, height, paramSize, GDT_Float32, papszOptions);

  // Metadata **************************

  double adfGeoTransform[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  // Places - latlon
  if (geomDefinedType == kLatLonGeom)
  {
    // double adfGeoTransformTmp[6] = {tlLon, aLon, 0, tlLat, 0, -aLat};  // decree
    // adfGeoTransform = adfGeoTransformTmp;
    adfGeoTransform[0] = tlLon;
    adfGeoTransform[1] = aLon;
    adfGeoTransform[3] = tlLat;
    adfGeoTransform[5] = -aLat;
  }
  else if (geomDefinedType == kYkjGeom)
  {
    NFmiPoint tlWorldXY = area->LatLonToWorldXY(area->TopLeftLatLon());
    NFmiPoint brWorldXY = area->LatLonToWorldXY(area->BottomRightLatLon());

    double aLon = (brWorldXY.X() - tlWorldXY.X()) / width;
    double aLat = (tlWorldXY.Y() - brWorldXY.Y()) / height;

    // double adfGeoTransformTmp[6] = {tlWorldXY.X(), aLon, 0, tlWorldXY.Y(), 0, -aLat};  // decree
    // adfGeoTransform = adfGeoTransformTmp;
    adfGeoTransform[0] = tlWorldXY.X();
    adfGeoTransform[1] = aLon;
    adfGeoTransform[3] = tlWorldXY.Y();
    adfGeoTransform[5] = -aLat;
  }
  else
  {
    if (geomDefinedType == kRotatedGeom)
    {
      double tlLon = area->WorldRect().Left();
      double tlLat = area->WorldRect().Top();

      double aLon = area->XScale() / width;
      double aLat = area->YScale() / height;

      // double adfGeoTransformTmp[6] = {tlLon, aLon, 0, tlLat, 0, aLat};  // decree
      // adfGeoTransform = adfGeoTransformTmp;
      adfGeoTransform[0] = tlLon;
      adfGeoTransform[1] = aLon;
      adfGeoTransform[3] = tlLat;
      adfGeoTransform[5] = aLat;
    }
    else
    {
      if (geomDefinedType == kStereoGeom)
      {
        NFmiPoint tlWorldXY = area->WorldRect().TopLeft();
        NFmiPoint brWorldXY = area->WorldRect().BottomRight();

        double aLon = (brWorldXY.X() - tlWorldXY.X()) / width;
        double aLat = (tlWorldXY.Y() - brWorldXY.Y()) / height;

        // double adfGeoTransformTmp[6] = {tlWorldXY.X(), aLon, 0, tlWorldXY.Y(), 0, -aLat};  //
        // decree
        // adfGeoTransform = adfGeoTransformTmp;
        adfGeoTransform[0] = tlWorldXY.X();
        adfGeoTransform[1] = aLon;
        adfGeoTransform[3] = tlWorldXY.Y();
        adfGeoTransform[5] = -aLat;
      }
    }
  }

  poDstDS->SetGeoTransform(adfGeoTransform);

  poDstDS->SetMetadataItem("SourceAgency", "FMI");
  NFmiTime timeNow = NFmiTime();
  poDstDS->SetMetadataItem("SourceDate", timeNow.ToStr(kYYYYMMDDHHMM));

  char *pszWKT = nullptr;
  // oSRS.SetFromUserInput(area->WKT().c_str());
  oSRS.exportToWkt(&pszWKT);
  poDstDS->SetProjection(pszWKT);

  printf("\n%s\n", pszWKT);

  int bandIndex = 1;

  poBand = poDstDS->GetRasterBand(bandIndex++);

  // Meta
  char paramIdent[10];
  poBand->SetMetadataItem("FmiId", paramIdent);
  poBand->SetMetadataItem("FmiName", theData->Param().GetParamName());

  // GDT_Int32 -or- GDT_Float32  2/4
  // int  *abyRaster;
  // abyRaster = fillIntRasterByQD(theData, theExternal, width, height, area);
  void *abyRaster;
  if (isIntDataType)
  {
    abyRaster = fillIntRasterByQD(theData, theExternal, width, height, area);
    // GDT_Int32   3/4
    poBand->RasterIO(GF_Write, 0, 0, width, height, abyRaster, width, height, GDT_Int32, 0, 0);
  }
  else
  {
    abyRaster = fillFloatRasterByQD(theData, theExternal, width, height, area);
    //  GDT_Float32  3/4
    poBand->RasterIO(GF_Write, 0, 0, width, height, abyRaster, width, height, GDT_Float32, 0, 0);
  }

  // External Band parameter for data int
  if (theExternal != 0)
  {
    poBand = poDstDS->GetRasterBand(bandIndex++);
    char paramIdent[10];
    poBand->SetMetadataItem("FmiId", paramIdent);
    poBand->SetMetadataItem("FmiName", theExternal->Param().GetParamName());
    // GDT_Int32 -or- GDT_Float32  4/4

    if (isIntDataType)
    {
      abyRaster = fillIntRasterByQD(theExternal, theData, width, height, area);
      // GDT_Int32   3/4
      poBand->RasterIO(GF_Write, 0, 0, width, height, abyRaster, width, height, GDT_Int32, 0, 0);
    }
    else
    {
      abyRaster = fillFloatRasterByQD(theExternal, theData, width, height, area);
      //  GDT_Float32  3/4
      poBand->RasterIO(GF_Write, 0, 0, width, height, abyRaster, width, height, GDT_Float32, 0, 0);
    }

    // abyRaster = fillIntRasterByQD(theExternal, theData, width, height, area);
    // abyRaster = fillFloatRasterByQD(theExternal, theData, width, height, area);
    // poBand->RasterIO( GF_Write, 0, 0, width, height,
    //	abyRaster, width, height, GDT_Float32 , 0, 0 );
  }

  // Close
  GDALClose((GDALDatasetH)poDstDS);
}

int *GeoTiffQD::fillIntRasterByQD(NFmiFastQueryInfo *theData,
                                  NFmiFastQueryInfo *theSecondData,
                                  int width,
                                  int height,
                                  const NFmiArea *area)
{
  int *abyRaster;

  NFmiArea *destArea = itsDestProjection;  //  CreteEpsgArea("EPSG:3035");

  const string def = "latlon:-179.0,-89.0,179.0,89.0";
  // NFmiArea *latLonArea = NFmiAreaFactory::Create(def)->Clone();

  abyRaster = (int *)CPLMalloc(sizeof(int) * width * height);

  theData->FirstLocation();

  NFmiDataMatrix<float> data;
  theData->Values(data);
  NFmiPoint *xy = new NFmiPoint(0, 0);

  // Second data for u/v - component
  NFmiDataMatrix<float> dataSecond;
  if (theSecondData != 0)
  {
    theSecondData->FirstLocation();
    theSecondData->Values(dataSecond);
  }

  bool is_rotlatlon = (area->Proj().GetString("proj") == std::string("ob_tran") &&
                       area->Proj().GetString("o_proj") == std::string("eqc") &&
                       area->Proj().GetString("towgs84") == std::string("0,0,0"));

  printf("Processing (int) with scale %f , QD to Gtiff raster convert for parameter %li\n",
         itsScale,
         theData->Param().GetParamIdent());
  int ref = height / 10;
  int refCount = 0;

  // const NFmiRotatedLatLonArea *rotArea = dynamic_cast<const NFmiRotatedLatLonArea*>(area);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      double value = (data[x][height - y - 1]);  // ok

      xy->X((x / (double)(width - 1)));
      xy->Y(1.0 - (height - y - 1) / (double)(height - 1));

      if (theData->Param().GetParamIdent() == 23 || theData->Param().GetParamIdent() == 24)
      {  // kFmiWindUMS||kFmiWindVMS
        if (area != 0)
        {
          if (value != 32700)
          {
            // const NFmiPoint latLon = area->ToLatLon(*xy);

            double valueSecond = (dataSecond[x][height - y - 1]);  // ok

            if (is_rotlatlon)
            {  // rotated project to destination
              // double azimuth1 = rotArea->TrueNorthAzimuth(latLon).ToRad();
              // double azimuth2 = destArea->TrueNorthAzimuth(latLon).ToRad();
              // double da = azimuth2 - azimuth1;

              double u = (theData->Param().GetParamIdent() == 23) ? value : valueSecond;
              double v = (theData->Param().GetParamIdent() == 23) ? valueSecond : value;

              // TODO: Is this a bug? Why are these variables not used? - Mika
              // double uu = u * cos(da) + v * sin(da);
              // double vv = v * cos(da) - u * sin(da);

              value = ((theData->Param().GetParamIdent() == 23) ? u : v);
            }
            else
            {                         // other projects to destination "not fixed yet"
              value = kFloatMissing;  //((theData->Param().GetParamIdent() == 23) ? u : v);
            }
          }
        }
      }
      else if (theData->Param().GetParamIdent() == 20)
      {  // kFmiWindDirection

        if (value != 32700)
        {
          // double valueSecond = (dataSecond[x][height-y-1] ); //ok

          const NFmiPoint latLon = area->ToLatLon(*xy);

          // if(is_rotlatlon){
          //	value = calculateTrueNorthAzimuthValue(value , rotArea, &latLon );
          //}

          value = calculateTrueNorthAzimuthValue(value, destArea, &latLon);
        }

        abyRaster[y * width + x] = (int)(value * itsScale);
      }
      else
      {
        const NFmiPoint latLon = area->ToLatLon(*xy);

        if (value != 32700)
        {
          if (isDrawGridLines)
          {
            drawGridLines(latLon, value);
          }
          abyRaster[y * width + x] = (int)(value * itsScale);
        }
        else
        {
          abyRaster[y * width + x] = 32700;
        }
      }
    }

    if (refCount++ > ref)
    {
      refCount = 0;
      printf(".. ");
    }
  }

  printf("Procesed \n");
  return abyRaster;
}

float *GeoTiffQD::fillFloatRasterByQD(NFmiFastQueryInfo *theData,
                                      NFmiFastQueryInfo *theSecondData,
                                      int width,
                                      int height,
                                      const NFmiArea *area)
{
  float *abyRaster;

  NFmiArea *destArea = itsDestProjection;  //  CreteEpsgArea("EPSG:3035");

  const string def = "latlon:-179.0,-89.0,179.0,89.0";
  // NFmiArea *latLonArea = NFmiAreaFactory::Create(def)->Clone();

  abyRaster = (float *)CPLMalloc(sizeof(float) * width * height);

  theData->FirstLocation();

  NFmiDataMatrix<float> data;
  theData->Values(data);
  NFmiPoint *xy = new NFmiPoint(0, 0);

  // Second data for u/v - component
  NFmiDataMatrix<float> dataSecond;
  if (theSecondData != 0)
  {
    theSecondData->FirstLocation();
    theSecondData->Values(dataSecond);
  }

  printf("Processing (float) with scale %f, QD to Gtiff raster convert for parameter %li \n",
         itsScale,
         theData->Param().GetParamIdent());
  int ref = height / 10;
  int refCount = 0;

  bool is_rotlatlon = (area->Proj().GetString("proj") == std::string("ob_tran") &&
                       area->Proj().GetString("o_proj") == std::string("eqc") &&
                       area->Proj().GetString("towgs84") == std::string("0,0,0"));

  // const NFmiRotatedLatLonArea *rotArea = dynamic_cast<const NFmiRotatedLatLonArea*>(area);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      double value = (data[x][height - y - 1]);  // ok

      xy->X((x / (double)(width - 1)));
      xy->Y(1.0 - (height - y - 1) / (double)(height - 1));

      if (theData->Param().GetParamIdent() == 23 || theData->Param().GetParamIdent() == 24)
      {  // kFmiWindUMS||kFmiWindVMS
        if (area != 0)
        {
          if (value != 32700)
          {
            // const NFmiPoint latLon = area->ToLatLon(*xy);

            double valueSecond = (dataSecond[x][height - y - 1]);  // ok

            if (is_rotlatlon)
            {  // rotated project to destination
              // double azimuth1 = rotArea->TrueNorthAzimuth(latLon).ToRad();
              // double azimuth2 = destArea->TrueNorthAzimuth(latLon).ToRad();
              // double da = azimuth2 - azimuth1;

              double u = (theData->Param().GetParamIdent() == 23) ? value : valueSecond;
              double v = (theData->Param().GetParamIdent() == 23) ? valueSecond : value;

              // TODO: is not using these variables a bug??
              // double uu = u * cos(da) + v * sin(da);
              // double vv = v * cos(da) - u * sin(da);

              value = ((theData->Param().GetParamIdent() == 23) ? u : v);
            }
            else
            {                         // other projects to destination "not fixed yet"
              value = kFloatMissing;  //((theData->Param().GetParamIdent() == 23) ? u : v);
            }
          }
        }
      }
      else if (theData->Param().GetParamIdent() == 20)
      {  // kFmiWindDirection

        if (value != 32700)
        {
          // double valueSecond = (dataSecond[x][height-y-1] ); //ok

          const NFmiPoint latLon = area->ToLatLon(*xy);

          // if(is_rotlatlon != 0){
          //	value = calculateTrueNorthAzimuthValue(value , rotArea, &latLon );
          //}

          value = calculateTrueNorthAzimuthValue(value, destArea, &latLon);
        }

        abyRaster[y * width + x] = value;
      }
      else
      {
        const NFmiPoint latLon = area->ToLatLon(*xy);

        if (value != 32700)
        {
          if (isDrawGridLines)
          {
            drawGridLines(latLon, value);
          }
          abyRaster[y * width + x] = value * itsScale;
        }
        else
        {
          abyRaster[y * width + x] = 32700;
        }
      }
    }

    if (refCount++ > ref)
    {
      refCount = 0;
      printf(".. ");
    }
  }

  printf("Procesed \n");
  return abyRaster;
}

/*
float * fillFloatRasterByQD(NFmiFastQueryInfo * theData, int width, int height)
{
        float * abyRaster;

        abyRaster = (float *)CPLMalloc(sizeof(float)*width*height);

        theData->FirstLocation();

        NFmiDataMatrix<float> data;
        theData->Values(data);

        printf( "Processing, QD to Gtiff raster convert for parameter %i \n",
theData->Param().GetParamName() );
    int ref = height/10;
        int refCount =0;
        for(int y=0; y<height; y++){
                for(int x=0; x<width; x++){
                        abyRaster[y*width+x] = data[x][height-y-1];
                }

                if(refCount++ > ref){
                        refCount=0;
                        printf( ".. " );
                }
        }

        printf( "Procesed \n" );
        return abyRaster;

}
*/

/*
int * fillIntRasterByQD(NFmiFastQueryInfo * theData, int width, int height)
{
        int * abyRaster;

        abyRaster = (int *)CPLMalloc(sizeof(int)*width*height);

        theData->FirstLocation();

        NFmiDataMatrix<float> data;
        theData->Values(data);

        printf( "Processing, QD to Gtiff raster convert for parameter %i \n",
theData->Param().GetParamIdent() );
    int ref = height/10;
        int refCount =0;
        for(int y=0; y<height; y++){
                for(int x=0; x<width; x++){
                        abyRaster[y*width+x] =  (int) (data[x][height-y-1] * 100.0)  ;
                }

                if(refCount++ > ref){
                        refCount=0;
                        printf( ".. " );
                }
        }

        printf( "Procesed \n" );
        return abyRaster;

}
*/
/*
NFmiArea * CreteEpsgArea(string epsgCode){

        NFmiArea *theArea = 0;

        if(epsgCode == "EPSG:3035"){
                const string def = "lambertequal,10,90,52:-10.6700, 31.55, 71.05";
                theArea = NFmiAreaFactory::Create(def)->Clone();
        }

        return theArea;
}
*/

// GeoTiffQD_App.cpp : Defines the entry point for the console application.
//

#ifdef _WIN32
#include <process.h>  //Only windows testing
#endif
#ifdef __linux__
#include <unistd.h>
#endif

void spawn_new_process(char *const *argv,
                       string tsrs,
                       string gdalParams,
                       GeomDefinedType geomType,
                       string tempFile,
                       string fileName);
int pid;

enum data_Type
{
  noneGDL,
  int32GDL,
  float32GDL
};
struct paramTypes
{
  int param = 0;
  int external = 0;
  float scale = 0;  // base
  data_Type dataType = noneGDL;
  data_Type dataTypeExternal = noneGDL;
};

static void usage()
{
  printf(
      "Usage: GeoTiffQD_App \n"
      "    -t_srs srs_def  (example EPSG:3035)  \n"
      "    -params 4,20,.. (if not use, make all params from qd)\n"
      "    -levels 10,11,12 -or- 700,850,925,... (if not use, make all level from qd)\n"
      "    -gdal_params gdal_par_def  ( \"-r bilinear\" )\n"
      "    [srcfile]\n"
      " \n"
      "example -params 4,20:21,47		param 20 and 21 yhdistetään samaan tif"
      "        -params x:y:k:v       x=param1, y=param2, k=scale, v=(0-int,1-float)"
      //"param 20 exands param 21 and added same a tif. (20 is fixed trueNorth)"

      //	-useInt32DataType (default=float32 )
      //  -params 4,20:21,47          param 20,21 yhdistetään samaan tiff:iin , tiedonimen 20_21
      //  -params 4,79::100.0;1       param 79 kerrotaan 100.0:lla
      //
      //	-params 4,20:21:1.0

      //  -test ajaa paketin vain kahdella ajanhetkellä

      //"    \nHow to make Projected conversion to tiff, exaple\n"
      //"        gdalwarp -s_srs proj.txt  -t_srs epsg:3035  -srcnodata 32700 -overwrite (rotated)
      //\n"
      //"        gdalwarp -s_srs epsg:4326  -t_srs epsg:3035  -srcnodata 32700 -overwrite (latlon)
      //\n"
      "\n");
  exit(1);
}

static bool fexists(const std::string &name)
{
  if (FILE *file = fopen(name.c_str(), "r"))
  {
    fclose(file);
    return true;
  }
  else
  {
    return false;
  }
}

static bool isSelectedParam(std::vector<paramTypes> ids, int id, paramTypes &paramValues)
{
  if (ids.size() == 0)
  {
    return true;
  }

  for (std::vector<paramTypes>::iterator it = ids.begin(); it != ids.end(); ++it)
  {
    if (it->param == id)
    {
      paramValues.param = it->param;
      paramValues.external = it->external;
      paramValues.scale = it->scale;
      paramValues.dataType = it->dataType;
      paramValues.dataTypeExternal = it->dataTypeExternal;

      return true;
    }
  }
  return false;
}

static bool isSelectedLevel(std::vector<int> ids, int id)
{
  if (ids.size() == 0)
  {
    return true;
  }

  for (std::vector<int>::iterator it = ids.begin(); it != ids.end(); ++it)
  {
    if (*it == id)
    {
      return true;
    }
  }
  return false;
}

static void writeRotatedLatLonWKT(const std::string &name)
{
  std::ostringstream ret;
  ret << std::setprecision(16) << "PROJCS[\"Fmi_Rotated_LatLon\","
      << "GEOGCS[\"Fmi_Sphere\","
      << "DATUM[\"Fmi_2007\",SPHEROID[\"Fmi_Sphere\",6371220,0]],"
      << "PRIMEM[\"Greenwich\",0],"
      << "UNIT[\"Degree\",0.0174532925199433]],"
      << "PARAMETER[\"latitude_of_origin\","
      << "0"
      << "],"
      << "PARAMETER[\"central_meridian\","
      << "-30"
      << "],"
      << "EXTENSION[\"PROJ4\",\"+proj=ob_tran +o_proj=eqc +lon_0=0 +o_lat_p=30 +R=57.29578 "
         "+wktext\"],"
      << "UNIT[\"Meter\",1.0]]";

  FILE *f;
  f = fopen(name.c_str(), "w");
  fprintf(f, "%s", ret.str().c_str());
  fclose(f);

  printf("Created file %s for RotatedLatLon projection", name.c_str());
}

#ifdef UNUSED
static void writeYkjWKT(const std::string &name)
{
  std::ostringstream ret;
  ret << std::setprecision(16) << "PROJCS[\"KKJ / Finland Uniform Coordinate System\","
      << "GEOGCS[\"KKJ\","
      << "DATUM[\"Kartastokoordinaattijarjestelma\",SPHEROID[\"International 1924\",6378388,297]],"
      << "PRIMEM[\"Greenwich\",0],"
      << "UNIT[\"Degree\",0.0174532925199433]],"
      << "PROJECTION[\"Transverse_Mercator\"],"
      << "PARAMETER[\"latitude_of_origin\","
      << "0"
      << "],"
      << "PARAMETER[\"central_meridian\","
      << "27"
      << "],"
      << "PARAMETER[\"false_easting\","
      << "3500000"
      << "],"
      << "PARAMETER[\"false_northing\","
      << "0"
      << "],"
      << "UNIT[\"metre\",1]]";

  FILE *f;
  f = fopen(name.c_str(), "w");
  fprintf(f, "%s", ret.str().c_str());
  fclose(f);

  printf("Created file %s for RotatedLatLon projection", name.c_str());
}
#endif

static void writeStereoWKT(const std::string &name, int centralLongitude)
{
  std::ostringstream ret;
  ret << std::setprecision(16) << "PROJCS[\"FMI_Polar_Stereographic\","
      << "GEOGCS[\"FMI_Sphere\","
      << "DATUM[\"FMI_2007\",SPHEROID[\"FMI_Sphere\",6371220,0]],"
      << "PRIMEM[\"Greenwich\",0],"
      << "UNIT[\"Degree\",0.0174532925199433]],"
      << "PROJECTION[\"Polar_Stereographic\"],"
      << "PARAMETER[\"latitude_of_origin\","
      << "60"
      << "],"
      << "PARAMETER[\"central_meridian\"," << centralLongitude << "],"
      << "PARAMETER[\"scale_factor\","
      << "90"
      << "],"
      << "PARAMETER[\"false_easting\","
      << "0"
      << "],"
      << "PARAMETER[\"false_northing\","
      << "0"
      << "],"
      << "UNIT[\"metre\",1]]";

  // PROJCS["FMI_Polar_Stereographic",GEOGCS["FMI_Sphere",DATUM["FMI_2007",SPHEROID["
  // FMI_Sphere",6371220,0]],PRIMEM["Greenwich",0],UNIT["Degree",0.0174532925199433]]
  //,PROJECTION["Polar_Stereographic"],PARAMETER["latitude_of_origin",60],PARAMETER[
  //"central_meridian",10],UNIT["Metre",1.0]]

  FILE *f;
  f = fopen(name.c_str(), "w");
  fprintf(f, "%s", ret.str().c_str());
  fclose(f);

  printf("Created file %s for RotatedLatLon projection", name.c_str());
}

int main(int argc, char *argv[])
{
  pid = getpid();
  /* ------------------------------------------------------------------- */
  /*      Parse arguments.                                               */
  /* ------------------------------------------------------------------- */
  // std::vector<int*> params;
  std::vector<paramTypes> params;
  std::vector<int> levels;
  string qdName;
  string tsrs;
  string gdalParams;

  bool isIntDataType = false;
  bool isTest = false;

  int i;

  for (i = 1; i < argc; i++)
  {
    if (EQUAL(argv[i], "-params"))
    {
      printf("params(%s)\n", argv[++i]);

      std::vector<char *> paramStrs;

      char *pch;
      pch = strtok(argv[i], ",");
      while (pch != nullptr)
      {
        paramStrs.push_back(pch);
        pch = strtok(nullptr, ",");
      }

      for (std::vector<char *>::iterator it = paramStrs.begin(); it != paramStrs.end(); ++it)
      {
        string paramStr = string(*it);

        size_t found = paramStr.find("::");
        if (found != string::npos)
        {
          paramStr.replace(found, 2, ":0:");
        }

        char *pch_scale_str = new char[paramStr.size() + 1];
        strcpy(pch_scale_str, paramStr.c_str());

        char *pch_scale = strtok(pch_scale_str, ":");
        int i = 0;
        // int * te = new int[3] ;
        paramTypes te;
        te.param = 0;
        te.external = 0;
        te.scale = 1.0;
        te.dataType = noneGDL;

        while (pch_scale != nullptr)
        {
          if (i == 0) te.param = atoi(pch_scale);
          if (i == 1) te.external = atoi(pch_scale);
          if (i == 2) te.scale = atof(pch_scale);
          i++;

          pch_scale = strtok(nullptr, ":");
        }
        params.push_back(te);
      }

      /*
                              char * pch;
                              pch = strtok (argv[i],",");
                               while (pch != nullptr)
                                {
                                      //int te = atoi(pch);
                                  int * te = new int[3] ;
                                      te[0] = 0;
                                      te[1] = 0;
                                      te[2] = 0;



                                      //string pch_scale_string = "20";
                                      //string pch_scale_string = "20:21";
                                      //string pch_scale_string = "20:21:99.9";
                                      //string pch_scale_string = "20::99.9";


                                      char * pchCopy = new char [sizeof(pch)+1];
                                      strcpy(pchCopy, "4");

                                      string pch_scale_string = pchCopy;

                                      size_t found = pch_scale_string.find("::");
                                      if(found!= string::npos)
                                      {
                                              cout << found;
                                              pch_scale_string.replace(found, 2, ":0:");
                                      }

                                      char * pch_scale_str = new char[pch_scale_string.size()+1];
                                      strcpy(pch_scale_str, pch_scale_string.c_str());

                                      char * pch_scale = strtok (pch_scale_str,":");
                                      int i=0;

                                      while (pch_scale != nullptr)
                                      {
                                              te[i++] = atoi(pch_scale);
                                              pch_scale = strtok (nullptr, ":");
                                      }

                                      te[0] = atoi(pch);
                                      te[1] = 0;
                                      te[2] = 0;

                                      params.push_back(te);
                                      pch = strtok (nullptr, ",");
                                }
      */
    }
    else if (EQUAL(argv[i], "-levels"))
    {
      printf("levels(%s)\n", argv[++i]);

      char *pch;
      pch = strtok(argv[i], ",");
      while (pch != nullptr)
      {
        int te = atoi(pch);
        levels.push_back(te);
        pch = strtok(nullptr, ",");
      }
    }
    else if (EQUAL(argv[i], "-t_srs"))
    {
      tsrs = argv[++i];
    }
    else if (EQUAL(argv[i], "-useInt32DataType"))
    {
      isIntDataType = true;
    }
    else if (EQUAL(argv[i], "-test"))
    {
      isTest = true;
    }
    else if (EQUAL(argv[i], "-gdal_params"))
    {
      gdalParams = argv[++i];
    }
    else
    {
      qdName = argv[i];
    }
  }

  if (argc >= 3)
  {
    GeoTiffQD geoTiffQD;
    NFmiQueryData *qd = new NFmiQueryData(qdName, true);

    if (qd != 0)
    {
      geoTiffQD.SetTestMode(isTest);

      if (tsrs == "EPSG:3035")
      {
        const string def = "lambertequal,10,90,52:-10.6700, 31.55, 71.05";
        geoTiffQD.DestinationProjection(NFmiAreaFactory::Create(def)->Clone());
      }
      else if (tsrs == "EPSG:2393")
      {
        const string def = "ykj:19.09,59.30,31.59,70.13";
        geoTiffQD.DestinationProjection(NFmiAreaFactory::Create(def)->Clone());
      }
      else if (tsrs == "EPSG:3995")
      {
        const string def =
            "stereographic,10.0,90.0,60.0:-22.3317013,29.0933476,63.624917,55.9863651";
        geoTiffQD.DestinationProjection(NFmiAreaFactory::Create(def)->Clone());
      }
      else if (tsrs == "EPSG:3067")
      {  // transverse mercator, ykj ~ same
        const string def = "ykj:19.09,59.30,31.59,70.13";
        geoTiffQD.DestinationProjection(NFmiAreaFactory::Create(def)->Clone());
      }

      NFmiFastQueryInfo *qi = new NFmiFastQueryInfo(qd);
      qi->Reset();

      int testCount = 0;
      while (qi->NextTime())
      {
        testCount++;
        qi->ResetParam();
        while (qi->NextParam(false))
        {
          paramTypes paramValues;
          if (isSelectedParam(params, qi->Param().GetParamIdent(), paramValues))
          {
            qi->ResetLevel();
            while (qi->NextLevel())
              if (isSelectedLevel(levels, qi->Level()->LevelValue()))
              {
                // string filename = argv[2];
                string filename;

                // Time stamp
                filename += static_cast<NFmiStaticTime>(qi->Time()).ToStr(kYYYYMMDDHHMM);

                filename += "_";

                // Producer id - or - name
                ostringstream ossProducer;
                ossProducer << qi->Param().GetProducer()->GetIdent();
                filename += ossProducer.str();
                // filename += qi->Param().GetProducer()->GetName();

                filename += "_";

                // Param id - or - name
                ostringstream ossParam;
                ossParam << qi->Param().GetParamIdent();
                filename += ossParam.str();
                filename += "_";
                if (paramValues.external > 0)
                {
                  ostringstream ossExternal;
                  ossExternal << paramValues.external;
                  filename += ossExternal.str();
                  filename += "_";
                }

                // Level id - or - name
                ostringstream ossLevel;
                ossLevel << qi->Level()->LevelTypeId();
                // filename += qi->Level().Get
                filename += ossLevel.str();

                filename += "_";

                // Level value
                ostringstream ossLevelValue;
                ossLevelValue << qi->Level()->LevelValue();
                filename += ossLevelValue.str();

                //
                filename += ".tif";
                string tmpname = "tmp_";
                tmpname += filename;

                NFmiFastQueryInfo *qiExternal = 0;
                if (paramValues.external > 0)
                {
                  qiExternal = (NFmiFastQueryInfo *)qi->Clone();
                  qiExternal->ResetParam();
                  while (qiExternal->NextParam(false))
                  {
                    if (static_cast<long>(qiExternal->Param().GetParamIdent()) ==
                        paramValues.external)
                    {
                      break;
                    }
                  }
                }

                if (!isTest || (isTest && testCount <= 2))
                {
                  GeomDefinedType geomType;
                  geomType = geoTiffQD.ConverQD2GeoTiff(
                      tmpname, qi, qiExternal, tsrs, isIntDataType, paramValues.scale);

                  // Only Windows testing start
                  spawn_new_process(argv, tsrs, gdalParams, geomType, tmpname, filename);
                }

              }  // if levels
          }      // if params
        }
      }  // // while time
    }    // if(qd != 0)

  }  // if(argc >= 3)
  else
  {
    usage();
  }

  // system("dir *.tif");

  /* We are still in the original process */
  printf("[%d] Original process is exiting.\n", pid);
  exit(EXIT_SUCCESS);

  return 0;
}  // main

bool isProjectedDone = false;
void spawn_new_process(char *const *argv,
                       string tsrs,
                       string gdalParams,
                       GeomDefinedType geomType,
                       string tempFile,
                       string fileName)
{
  string gdalWrap = "gdalwarp ";
  if (geomType == kRotatedGeom)
  {
    if (isProjectedDone || !fexists("projForRotatedLatLon.txt"))
    {
      writeRotatedLatLonWKT("projForRotatedLatLon.txt");
    }

    gdalWrap += " -s_srs projForRotatedLatLon.txt ";
  }
  else

      if (geomType == kLatLonGeom)
  {
    gdalWrap += " -s_srs epsg:4326 ";
  }
  else if (geomType == kStereoGeom || geomType == kStereoGeom10 || geomType == kStereoGeom20)
  {
    string projFile = "projForStereo.txt";
    int centralLongitude = 0;

    if (geomType == kStereoGeom10)
    {
      centralLongitude = 10;
      projFile = "projForStereo10.txt";
    }
    if (geomType == kStereoGeom20)
    {
      centralLongitude = 20;
      projFile = "projForStereo20.txt";
    }

    if (!fexists(projFile))
    {
      writeStereoWKT(projFile, centralLongitude);
    }
    gdalWrap += " -s_srs ";
    gdalWrap += projFile;
  }
  else if (geomType == kYkjGeom)
  {
    // if(isRotatedDone || !fexists("projForYkj.txt")){
    //	writeYkjWKT("projForYkj.txt");
    //}
    gdalWrap += " -s_srs epsg:2393 ";
    // gdalWrap += " -s_srs  ";
  }
  else
  {
    return;
  }

  gdalWrap += " -t_srs ";  //"epsg:3035 ";
  gdalWrap += tsrs;
  gdalWrap += " ";
  gdalWrap += gdalParams;
  gdalWrap += " -srcnodata 32700 -overwrite ";
  gdalWrap += tempFile;
  gdalWrap += " ";
  gdalWrap += fileName;

#ifdef _WIN32

  printf("%s\n\n", gdalWrap.c_str());

  string gdalDel = "del ";
  gdalDel += tempFile;

  system(gdalWrap.c_str());
  // system(gdalDel.c_str());

#endif

#ifdef __linux__

  string gdalDel = "rm ";
  gdalDel += tempFile;

  system(gdalWrap.c_str());
  system(gdalDel.c_str());

#endif
}
