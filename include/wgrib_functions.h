
#ifndef WGRIB_FUNCTIONS_H
#define WGRIB_FUNCTIONS_H

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef INT2
#define INT2(a, b) ((1 - (int)((unsigned)(a & 0x80) >> 6)) * (int)(((a & 0x7f) << 8) + b))
#endif

#define BDS_LEN(bds) ((int)((bds[0] << 16) + (bds[1] << 8) + bds[2]))
#define BDS_Flag(bds) (bds[3])

#define BDS_Grid(bds) ((bds[3] & 128) == 0)
#define BDS_Harmonic(bds) (bds[3] & 128)

#define BDS_Packing(bds) ((bds[3] & 64) != 0)
#define BDS_SimplePacking(bds) ((bds[3] & 64) == 0)
#define BDS_ComplexPacking(bds) ((bds[3] & 64) != 0)

#define BDS_OriginalType(bds) ((bds[3] & 32) != 0)
#define BDS_OriginalFloat(bds) ((bds[3] & 32) == 0)
#define BDS_OriginalInt(bds) ((bds[3] & 32) != 0)

#define BDS_MoreFlags(bds) ((bds[3] & 16) != 0)
#define BDS_UnusedBits(bds) ((int)(bds[3] & 15))

#define BDS_BinScale(bds) INT2(bds[4], bds[5])

#define BDS_RefValue(bds) (ibm2flt(bds + 6))
#define BDS_NumBits(bds) ((int)bds[10])

#define BDS_Harmonic_RefValue(bds) (ibm2flt(bds + 11))

#define BDS_DataStart(bds) ((int)(11 + BDS_MoreFlags(bds) * 3))

/* breaks if BDS_NumBits(bds) == 0 */
#define BDS_NValues(bds) \
  (((BDS_LEN(bds) - BDS_DataStart(bds)) * 8 - BDS_UnusedBits(bds)) / BDS_NumBits(bds))

/*
#define BDS_NValues(bds)        ((BDS_NumBits(bds) == 0) ? 0 : \
                                (((BDS_LEN(bds) - BDS_DataStart(bds))*8 - \
                                BDS_UnusedBits(bds)) / BDS_NumBits(bds)))
*/

/* undefined value -- if bitmap */
#define UNDEFINED 9.999e20

/* version 1.2 of grib headers  w. ebisuzaki */

#define BMS_LEN(bms) ((bms) == nullptr ? 0 : (bms[0] << 16) + (bms[1] << 8) + bms[2])
#define BMS_UnusedBits(bms) ((bms) == nullptr ? 0 : bms[3])
#define BMS_StdMap(bms) ((bms) == nullptr ? 0 : ((bms[4] << 8) + bms[5]))
#define BMS_bitmap(bms) ((bms) == nullptr ? nullptr : (bms) + 6)
#define BMS_nxny(bms) \
  ((((bms) == nullptr) || BMS_StdMap(bms)) ? 0 : (BMS_LEN(bms) * 8 - 48 - BMS_UnusedBits(bms)))
/* cnames_file.c */

/* search order for parameter names
 *
 * #define P_TABLE_FIRST
 * look at external parameter table first
 *
 * otherwise use builtin NCEP-2 or ECMWF-160 first
 */
/* #define P_TABLE_FIRST */

/* search order for external parameter table
 * 1) environment variable GRIBTAB
 * 2) environment variable gribtab
 * 3) the file 'gribtab' in current directory
 */

/* cnames.c */
/* then default values */
const char *k5toa(unsigned char *pds);
const char *k5_comments(unsigned char *pds);
int setup_user_table(int center, int subcenter, int ptable);

struct ParmTable
{
  const char *name, *comment;
};

/* version 1.4.3 of grib headers  w. ebisuzaki */
/* this version is incomplete */
/* 5/00 - dx/dy or di/dj controlled by bit 1 of resolution byte */
/* 8/00 - dx/dy or di/dj for polar and lambert not controlled by res. byte */
/* Added headers for the triangular grid of the gme model of DWD
         Helmut P. Frank, 13.09.2001 */
/* Clean up of triangular grid properties access and added spectral information
         Luis Kornblueh, 27.03.2002 */

#ifndef INT3
#define INT3(a, b, c) \
  ((1 - (int)((unsigned)(a & 0x80) >> 6)) * (int)(((a & 127) << 16) + (b << 8) + c))
#endif
#ifndef INT2
#define INT2(a, b) ((1 - (int)((unsigned)(a & 0x80) >> 6)) * (int)(((a & 127) << 8) + b))
#endif

#ifndef UINT4
#define UINT4(a, b, c, d) ((int)((a << 24) + (b << 16) + (c << 8) + (d)))
#endif

#ifndef UINT3
#define UINT3(a, b, c) ((int)((a << 16) + (b << 8) + (c)))
#endif

#ifndef UINT2
#define UINT2(a, b) ((int)((a << 8) + (b)))
#endif

#define GDS_Len1(gds) (gds[0])
#define GDS_Len2(gds) (gds[1])
#define GDS_Len3(gds) (gds[2])
#define GDS_LEN(gds) ((int)((gds[0] << 16) + (gds[1] << 8) + gds[2]))

#define GDS_NV(gds) (gds[3])
#define GDS_DataType(gds) (gds[5])

#define GDS_LatLon(gds) (gds[5] == 0)
#define GDS_Mercator(gds) (gds[5] == 1)
#define GDS_Gnomonic(gds) (gds[5] == 2)
#define GDS_Lambert(gds) (gds[5] == 3)
#define GDS_Gaussian(gds) (gds[5] == 4)
#define GDS_Polar(gds) (gds[5] == 5)
#define GDS_RotLL(gds) (gds[5] == 10)
#define GDS_Harmonic(gds) (gds[5] == 50)
#define GDS_Triangular(gds) (gds[5] == 192)
#define GDS_ssEgrid(gds) (gds[5] == 201)   /* semi-staggered E grid */
#define GDS_fEgrid(gds) (gds[5] == 202)    /* filled E grid */
#define GDS_ss2dEgrid(gds) (gds[5] == 203) /* semi-staggered E grid 2 d*/

#define GDS_has_dy(mode) ((mode)&128)
#define GDS_LatLon_nx(gds) ((int)((gds[6] << 8) + gds[7]))
#define GDS_LatLon_ny(gds) ((int)((gds[8] << 8) + gds[9]))
#define GDS_LatLon_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_LatLon_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_LatLon_mode(gds) (gds[16])
#define GDS_LatLon_La2(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_LatLon_Lo2(gds) INT3(gds[20], gds[21], gds[22])

#define GDS_LatLon_dx(gds) (gds[16] & 128 ? INT2(gds[23], gds[24]) : 0)
#define GDS_LatLon_dy(gds) (gds[16] & 128 ? INT2(gds[25], gds[26]) : 0)
#define GDS_Gaussian_nlat(gds) ((gds[25] << 8) + gds[26])

#define GDS_LatLon_scan(gds) (gds[27])

#define GDS_Polar_nx(gds) ((gds[6] << 8) + gds[7])
#define GDS_Polar_ny(gds) ((gds[8] << 8) + gds[9])
#define GDS_Polar_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_Polar_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_Polar_mode(gds) (gds[16])
#define GDS_Polar_Lov(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_Polar_scan(gds) (gds[27])
#define GDS_Polar_Dx(gds) INT3(gds[20], gds[21], gds[22])
#define GDS_Polar_Dy(gds) INT3(gds[23], gds[24], gds[25])
#define GDS_Polar_pole(gds) ((gds[26] & 128) == 128)

#define GDS_Lambert_nx(gds) ((gds[6] << 8) + gds[7])
#define GDS_Lambert_ny(gds) ((gds[8] << 8) + gds[9])
#define GDS_Lambert_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_Lambert_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_Lambert_mode(gds) (gds[16])
#define GDS_Lambert_Lov(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_Lambert_dx(gds) INT3(gds[20], gds[21], gds[22])
#define GDS_Lambert_dy(gds) INT3(gds[23], gds[24], gds[25])
#define GDS_Lambert_NP(gds) ((gds[26] & 128) == 0)
#define GDS_Lambert_scan(gds) (gds[27])
#define GDS_Lambert_Latin1(gds) INT3(gds[28], gds[29], gds[30])
#define GDS_Lambert_Latin2(gds) INT3(gds[31], gds[32], gds[33])
#define GDS_Lambert_LatSP(gds) INT3(gds[34], gds[35], gds[36])
#define GDS_Lambert_LonSP(gds) INT3(gds[37], gds[37], gds[37])

#define GDS_ssEgrid_n(gds) UINT2(gds[6], gds[7])
#define GDS_ssEgrid_n_dum(gds) UINT2(gds[8], gds[9])
#define GDS_ssEgrid_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_ssEgrid_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_ssEgrid_mode(gds) (gds[16])
#define GDS_ssEgrid_La2(gds) UINT3(gds[17], gds[18], gds[19])
#define GDS_ssEgrid_Lo2(gds) UINT3(gds[20], gds[21], gds[22])
#define GDS_ssEgrid_di(gds) (gds[16] & 128 ? INT2(gds[23], gds[24]) : 0)
#define GDS_ssEgrid_dj(gds) (gds[16] & 128 ? INT2(gds[25], gds[26]) : 0)
#define GDS_ssEgrid_scan(gds) (gds[27])

#define GDS_fEgrid_n(gds) UINT2(gds[6], gds[7])
#define GDS_fEgrid_n_dum(gds) UINT2(gds[8], gds[9])
#define GDS_fEgrid_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_fEgrid_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_fEgrid_mode(gds) (gds[16])
#define GDS_fEgrid_La2(gds) UINT3(gds[17], gds[18], gds[19])
#define GDS_fEgrid_Lo2(gds) UINT3(gds[20], gds[21], gds[22])
#define GDS_fEgrid_di(gds) (gds[16] & 128 ? INT2(gds[23], gds[24]) : 0)
#define GDS_fEgrid_dj(gds) (gds[16] & 128 ? INT2(gds[25], gds[26]) : 0)
#define GDS_fEgrid_scan(gds) (gds[27])

#define GDS_ss2dEgrid_nx(gds) UINT2(gds[6], gds[7])
#define GDS_ss2dEgrid_ny(gds) UINT2(gds[8], gds[9])
#define GDS_ss2dEgrid_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_ss2dEgrid_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_ss2dEgrid_mode(gds) (gds[16])
#define GDS_ss2dEgrid_La2(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_ss2dEgrid_Lo2(gds) INT3(gds[20], gds[21], gds[22])
#define GDS_ss2dEgrid_di(gds) (gds[16] & 128 ? INT2(gds[23], gds[24]) : 0)
#define GDS_ss2dEgrid_dj(gds) (gds[16] & 128 ? INT2(gds[25], gds[26]) : 0)
#define GDS_ss2dEgrid_scan(gds) (gds[27])

#define GDS_Merc_nx(gds) UINT2(gds[6], gds[7])
#define GDS_Merc_ny(gds) UINT2(gds[8], gds[9])
#define GDS_Merc_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_Merc_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_Merc_mode(gds) (gds[16])
#define GDS_Merc_La2(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_Merc_Lo2(gds) INT3(gds[20], gds[21], gds[22])
#define GDS_Merc_Latin(gds) INT3(gds[23], gds[24], gds[25])
#define GDS_Merc_scan(gds) (gds[27])
#define GDS_Merc_dx(gds) (gds[16] & 128 ? INT3(gds[28], gds[29], gds[30]) : 0)
#define GDS_Merc_dy(gds) (gds[16] & 128 ? INT3(gds[31], gds[32], gds[33]) : 0)

/* rotated Lat-lon grid */

#define GDS_RotLL_nx(gds) UINT2(gds[6], gds[7])
#define GDS_RotLL_ny(gds) UINT2(gds[8], gds[9])
#define GDS_RotLL_La1(gds) INT3(gds[10], gds[11], gds[12])
#define GDS_RotLL_Lo1(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_RotLL_mode(gds) (gds[16])
#define GDS_RotLL_La2(gds) INT3(gds[17], gds[18], gds[19])
#define GDS_RotLL_Lo2(gds) INT3(gds[20], gds[21], gds[22])
#define GDS_RotLL_dx(gds) (gds[16] & 128 ? INT2(gds[23], gds[24]) : 0)
#define GDS_RotLL_dy(gds) (gds[16] & 128 ? INT2(gds[25], gds[26]) : 0)
#define GDS_RotLL_scan(gds) (gds[27])
#define GDS_RotLL_LaSP(gds) INT3(gds[32], gds[33], gds[34])
#define GDS_RotLL_LoSP(gds) INT3(gds[35], gds[36], gds[37])
#define GDS_RotLL_RotAng(gds) ibm2flt(&(gds[38]))

/* Triangular grid of DWD */
#define GDS_Triangular_ni2(gds) INT2(gds[6], gds[7])
#define GDS_Triangular_ni3(gds) INT2(gds[8], gds[9])
#define GDS_Triangular_ni(gds) INT3(gds[13], gds[14], gds[15])
#define GDS_Triangular_nd(gds) INT3(gds[10], gds[11], gds[12])

/* Harmonics data */
#define GDS_Harmonic_nj(gds) ((int)((gds[6] << 8) + gds[7]))
#define GDS_Harmonic_nk(gds) ((int)((gds[8] << 8) + gds[9]))
#define GDS_Harmonic_nm(gds) ((int)((gds[10] << 8) + gds[11]))
#define GDS_Harmonic_type(gds) (gds[12])
#define GDS_Harmonic_mode(gds) (gds[13])

/* index of NV and PV */
#define GDS_PV(gds) ((gds[3] == 0) ? -1 : (int)gds[4] - 1)
#define GDS_PL(gds) ((gds[4] == 255) ? -1 : (int)gds[3] * 4 + (int)gds[4] - 1)

enum Def_NCEP_Table
{
  rean,
  opn,
  rean_nowarn,
  opn_nowarn
};

unsigned char *seek_grib(
    FILE *file, long *pos, long *len_grib, unsigned char *buffer, unsigned int buf_len);

int read_grib(FILE *file, long pos, long len_grib, unsigned char *buffer);

double ibm2flt(unsigned char *ibm);

void BDS_unpack(float *flt,
                unsigned char *bds,
                unsigned char *bitmap,
                int n_bits,
                int n,
                double ref,
                double scale);

double int_power(double x, int y);

int flt2ieee(float x, unsigned char *ieee);

int wrtieee(float *array, int n, int header, FILE *output);
int wrtieee_header(unsigned int n, FILE *output);

void levels(int, int, int);

void PDStimes(int time_range, int p1, int p2, int time_unit);

int missing_points(unsigned char *bitmap, int n);

void EC_ext(unsigned char *pds, char *prefix, char *suffix);

int GDS_grid(unsigned char *gds,
             unsigned char *bds,
             int *nx,
             int *ny,
             long int *nxny,
             std::vector<long> &theVariableLengthRows,
             long *theUsedOutputGridRowLength);

void GDS_prt_thin_lon(unsigned char *gds);

void GDS_winds(unsigned char *gds, int verbose);

int PDS_date(unsigned char *pds, int option, int verf_time);

int add_time(int *year, int *month, int *day, int *hour, int dtime, int unit);

int verf_time(unsigned char *pds, int *year, int *month, int *day, int *hour);

void print_pds(unsigned char *pds, int print_PDS, int print_PDS10, int verbose);
void print_gds(unsigned char *gds, int print_GDS, int print_GDS10, int verbose);

void ensemble(unsigned char *pds, int mode);
/* version 3.4 of grib headers  w. ebisuzaki */
/* this version is incomplete */
/* add center DWD    Helmut P. Frank */
/* 10/02 add center CPTEC */

#ifndef INT2
#define INT2(a, b) ((1 - (int)((unsigned)(a & 0x80) >> 6)) * (int)(((a & 0x7f) << 8) + b))
#endif

#define PDS_Len1(pds) (pds[0])
#define PDS_Len2(pds) (pds[1])
#define PDS_Len3(pds) (pds[2])
#define PDS_LEN(pds) ((int)((pds[0] << 16) + (pds[1] << 8) + pds[2]))
#define PDS_Vsn(pds) (pds[3])
#define PDS_Center(pds) (pds[4])
#define PDS_Model(pds) (pds[5])
#define PDS_Grid(pds) (pds[6])
#define PDS_HAS_GDS(pds) ((pds[7] & 128) != 0)
#define PDS_HAS_BMS(pds) ((pds[7] & 64) != 0)
#define PDS_PARAM(pds) (pds[8])
#define PDS_L_TYPE(pds) (pds[9])
#define PDS_LEVEL1(pds) (pds[10])
#define PDS_LEVEL2(pds) (pds[11])

#define PDS_KPDS5(pds) (pds[8])
#define PDS_KPDS6(pds) (pds[9])
#define PDS_KPDS7(pds) ((int)((pds[10] << 8) + pds[11]))

/* this requires a 32-bit default integer machine */
#define PDS_Field(pds) ((pds[8] << 24) + (pds[9] << 16) + (pds[10] << 8) + pds[11])

#define PDS_Year(pds) (pds[12])
#define PDS_Month(pds) (pds[13])
#define PDS_Day(pds) (pds[14])
#define PDS_Hour(pds) (pds[15])
#define PDS_Minute(pds) (pds[16])
#define PDS_ForecastTimeUnit(pds) (pds[17])
#define PDS_P1(pds) (pds[18])
#define PDS_P2(pds) (pds[19])
#define PDS_TimeRange(pds) (pds[20])
#define PDS_NumAve(pds) ((int)((pds[21] << 8) + pds[22]))
#define PDS_NumMissing(pds) (pds[23])
#define PDS_Century(pds) (pds[24])
#define PDS_Subcenter(pds) (pds[25])
#define PDS_DecimalScale(pds) INT2(pds[26], pds[27])
/* old #define PDS_Year4(pds)   (pds[12] + 100*(pds[24] - (pds[12] != 0))) */
#define PDS_Year4(pds) (pds[12] + 100 * (pds[24] - 1))

/* various centers */
#define NMC 7
#define ECMWF 98
#define DWD 78
#define CMC 54
#define CPTEC 46

/* ECMWF Extensions */

#define PDS_EcLocalId(pds) (PDS_LEN(pds) >= 41 ? (pds[40]) : 0)
#define PDS_EcClass(pds) (PDS_LEN(pds) >= 42 ? (pds[41]) : 0)
#define PDS_EcType(pds) (PDS_LEN(pds) >= 43 ? (pds[42]) : 0)
#define PDS_EcStream(pds) (PDS_LEN(pds) >= 45 ? (INT2(pds[43], pds[44])) : 0)

#define PDS_EcENS(pds) \
  (PDS_LEN(pds) >= 52 && pds[40] == 1 && pds[43] * 256 + pds[44] == 1035 && pds[50] != 0)
#define PDS_EcFcstNo(pds) (pds[50])
#define PDS_EcNoFcst(pds) (pds[51])

/* NCEP Extensions */

#define PDS_NcepENS(pds) (PDS_LEN(pds) >= 44 && pds[25] == 2 && pds[40] == 1)
#define PDS_NcepFcstType(pds) (pds[41])
#define PDS_NcepFcstNo(pds) (pds[42])
#define PDS_NcepFcstProd(pds) (pds[43])

/* time units */

#define MINUTE 0
#define HOUR 1
#define DAY 2
#define MONTH 3
#define YEAR 4
#define DECADE 5
#define NORMAL 6
#define CENTURY 7
#define HOURS3 10
#define HOURS6 11
#define HOURS12 12
#define SECOND 254

#define VERSION                                                                           \
  "v1.8.0.3k (7-25-03) Wesley Ebisuzaki\n\t\tDWD-tables 2,201-203 (9-13-2001) Helmut P. " \
  "Frank\n\t\tspectral: Luis Kornblueh (MPI)"

#define CHECK_GRIB

/*
 * wgrib.c extract/inventory grib records
 *
 *                              Wesley Ebisuzaki
 *
 * 11/94 - v1.0
 * 11/94 - v1.1: arbitary size grids, -i option
 * 11/94 - v1.2: bug fixes, ieee option, more info
 * 1/95  - v1.2.4: fix headers for SUN acc
 * 2/95  - v1.2.5: add num_ave in -s listing
 * 2/95  - v1.2.6: change %d to %ld
 * 2/95  - v1.2.7: more output, added some polar stereographic support
 * 2/95  - v1.2.8: max min format changed %f to %g, tidying up more info
 * 3/95  - v1.3.0: fix bug with bitmap, allow numbers > UNDEFINED
 * 3/95  - v1.3.1: print number of missing points (verbose)
 * 3/95  - v1.3.2: -append option added
 * 4/95  - v1.3.2a,b: more output, polar stereo support (-V option)
 * 4/95  - v1.3.3: added ECMWF parameter table (prelim)
 * 6/95  - v1.3.4: nxny from BDS rather than gds?
 * 9/95  - v1.3.4d: speedup in grib write
 * 11/95 - v1.3.4f: new ECMWF parameter table (from Mike Fiorino), EC logic
 * 2/96  - v1.3.4g-h: prelim fix for GDS-less grib files
 * 2/96  - v1.3.4i: faster missing(), -V: "pos n" -> "n" (field 2)
 * 3/96  - v1.4: fix return code (!inventory), and short records near EOF
 * 6/96  - v1.4.1a: faster grib->binary decode, updated ncep parameter table, mod. in clim. desc
 * 7/96  - v1.5.0: parameter-table aware, -v option changed, added "comments"
 *                 increased NTRY to 100 in seek_grib
 * 11/96 - v1.5.0b: added ECMWF parameter table 128
 * 1/97 - v1.5.0b2: if nxny != nx*ny { nx = nxny; ny = 1 }
 * 3/97 - v1.5.0b5: added: -PDS -GDS, Lambert Conformal
 * 3/97 - v1.5.0b6: added: -verf
 * 4/97 - v1.5.0b7: added -PDS10, -GDS10 and enhanced -PDS -GDS
 * 4/97 - v1.5.0b8: "bitmap missing x" -> "bitmap: x undef"
 * 5/97 - v1.5.0b9: thinned grids meta data
 * 5/97 - v1.5.0b10: changed 0hr fcst to anal for TR=10 and P1=P2=0
 * 5/97 - v1.5.0b10: added -H option
 * 6/97 - v1.5.0b12: thinned lat-long grids -V option
 * 6/97 - v1.5.0b13: -4yr
 * 6/97 - v1.5.0b14: fix century mark Y=100 not 0
 * 7/97 - v1.5.0b15: add ncep opn grib table
 * 12/97 - v1.6.1.a: made ncep_opn the default table
 * 12/97 - v1.6.1.b: changed 03TOT to O3TOT in operational ncep table
 * 1/98  - v1.6.2: added Arakawa E grid meta-data
 * 1/98  - v1.6.2.1: added some mode data, Scan -> scan
 * 4/98  - v1.6.2.4: reanalysis id code: subcenter==0 && process==180
 * 5/98  - v1.6.2.5: fix -H code to write all of GDS
 * 7/98  - v1.7: fix decoding bug for bitmap and no. bits > 24 (theoretical bug)
 * 7/98  - v1.7.0.b1: add km to Mercator meta-data
 * 5/99  - v1.7.2: bug with thinned grids & bitmaps (nxny != nx*ny)
 * 5/99  - v1.7.3: updated NCEP opn grib table
 * 8/99  - v1.7.3.1: updated level information
 * 9/00  - v1.7.3.4a: check for missing grib file
 * 2/01  - v1.7.3.5: handle data with precision greater than 31 bits
 * 8/01  - vDWD   : added DWD GRIB tables 201, 202, 203, Helmut P. Frank
 * 9/01  - vDWD   : added output "Triangular grid", Helmut P. Frank
 * 9/01  - v1.7.4: merged Hemut P. Frank's changes to current wgrib source code
 * 3/02  - vMPIfM: added support for spectral data type
 * 4/02  - v1.8:   merge vMPIfM changes, some fixes/generalizations
 * 10/02  - v1.8.0.1: added cptec table 254
 * 10/02  - v1.8.0.2: no test of grib test if no gds, level 117 redone
 * 10/02  - v1.8.0.3: update ncep_opn grib and levels
 * 11/02  - v1.8.0.3a: updated ncep_opn and ncep table 129
 *
 */

/*
 * MSEEK = I/O buffer size for seek_grib
 */

#define MSEEK 1024
#define BUFF_ALLOC0 40000

#ifndef UNIX
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))
#endif
#endif

#ifndef DEF_T62_NCEP_TABLE
#define DEF_T62_NCEP_TABLE rean
#endif

// siirsin tämän pois headerista linkkaus ongelman takia
// enum Def_NCEP_Table def_ncep_table = DEF_T62_NCEP_TABLE;

#endif  // WGRIB_FUNCTIONS_H
