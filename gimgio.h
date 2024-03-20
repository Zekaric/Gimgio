/******************************************************************************

file:       gimgio.h
author:     Robbert de Groot
copyright:  2008-2008, Robbert de Groot

description:
Image IO routines.  Handles the standard image formats like png, jpeg,
bmp.  This will never be as complete as LEADTOOLs or image magic.  This
is meant only to handle the 'major' formats and leave the rest.

******************************************************************************/

#if !defined(GIMGIOH)
#define      GIMGIOH

/* C++ include */
#if defined(__cplusplus)
extern "C"
{
#endif
/***************/

#if !defined(GIMGIO_DLL)
#  define      gimgioAPI
#else
#  if defined(GIMGIO_EXPORTS)
#     define   gimgioAPI  __declspec(dllexport)
#  else
#     define   gimgioAPI  __declspec(dllimport)
#  endif
#endif

/******************************************************************************
include: 
******************************************************************************/
#include "grl.h"   

/******************************************************************************
constant: 
******************************************************************************/
typedef enum
{
   gimgioOpenNONE,
   // Read only
   gimgioOpenREAD,
   // Write a brand new file.
   gimgioOpenWRITE,
} GimgioOpenMode;

typedef enum
{
   gimgioTypeNONE,

   // channel size.
   gimgioTypeB1         = 0x00000001,
   gimgioTypeB2         = 0x00000002,
   gimgioTypeB4         = 0x00000004,
   gimgioTypeN1         = 0x00000010,
   gimgioTypeN2         = 0x00000020,
   gimgioTypeN4         = 0x00000040,
   gimgioTypeN8         = 0x00000080,
   gimgioTypeR4         = 0x00000100,
   gimgioTypeR8         = 0x00000200,

   // channels.
   gimgioTypeBLACK      = 0x10000000,
   gimgioTypeRGB        = 0x03000000,
   gimgioTypeALPHA      = 0x00001000,

   // real channel sizes.
   gimgioTypeBIT        = 0x0000000f,
   gimgioTypeNATURAL    = 0x000000f0,
   gimgioTypeREAL       = 0x00000f00
} GimgioType;

typedef enum
{
   gimgioFormatNONE,
   gimgioFormatBMP,
   gimgioFormatGIF,
   gimgioFormatGRAW, // GRL RAW format.
   gimgioFormatJPG,
   gimgioFormatPNG,
   gimgioFormatPPM,
   gimgioFormatRLE,
   gimgioFormatTRG,
   gimgioFormatTIFF,
} GimgioFormat;

/******************************************************************************
type: 
******************************************************************************/
typedef struct _Gimgio
{
#if defined(gimgioLOCAL_INCLUDE)
   TIFF           *tiffFile;
#else
   void           *tiffFile;
#endif
   Gfile          *file;
   Gs             *fileName;
   GimgioOpenMode  mode;
   GimgioFormat    format;
   GimgioType      typeFile,
                   typePixel;

   Gindex          imageCount;
   Gindex          imageIndex;
   Gcount          width;
   Gcount          height;
   Gindex          row;
   Gr              compression;
   
   // specific to the image formats
   void           *data;
   void          (*DestroyContent)(struct _Gimgio * const img);
   Gb            (*GetPixelRow)(   struct _Gimgio * const img, void * const pixel);
   Gb            (*ReadStart)(     struct _Gimgio * const img);
   Gb            (*SetImageIndex)( struct _Gimgio * const img, Gindex const index);
   Gb            (*SetPixelRow)(   struct _Gimgio * const img, void * const pixel);
   Gb            (*SetTypeFile)(   struct _Gimgio * const img);
} Gimgio;

/******************************************************************************
prototype: 
******************************************************************************/
#define gimgioOpen(file, mode, format) \
   ((Gimgio *) gleakCreate((void *) gimgioOpen_(file, mode, format), gsizeof(Gimgio)))

gimgioAPI void         gimgioClose(             Gimgio       * const img);
gimgioAPI void         gimgioConvert(           Gi4 const width, GimgioType const inType, void const * const in, GimgioType const outType, void * const out);

gimgioAPI Gr           gimgioGetCompression(    Gimgio const * const img);
gimgioAPI GimgioFormat gimgioGetFormat(         Gimgio const * const img);
gimgioAPI GimgioFormat gimgioGetFormatFromName( Gpath const * const path);
gimgioAPI Gcount       gimgioGetHeight(         Gimgio const * const img);
gimgioAPI Gcount       gimgioGetImageCount(     Gimgio const * const img);
gimgioAPI Gindex       gimgioGetImageIndex(     Gimgio const * const img);
gimgioAPI Gb           gimgioGetPixelRow(       Gimgio       * const img, void * const pixel);
gimgioAPI Gb           gimgioGetPixelRowAll(    Gimgio       * const img, Gn1 * const pixel);
gimgioAPI Gsize        gimgioGetPixelSize(      GimgioType const type, Gi4 const width);
gimgioAPI void         gimgioGetPixelAtN(       GimgioType const type, Gi4 const index, void * const pixel, Gn4 * const r, Gn4 * const g, Gn4 * const b, Gn4 * const a);
gimgioAPI void         gimgioGetPixelAtR(       GimgioType const type, Gi4 const index, void * const pixel, Gr * const r, Gr * const g, Gr * const b, Gr * const a);
gimgioAPI Gindex       gimgioGetRow(            Gimgio const * const img);
gimgioAPI GimgioType   gimgioGetTypeFile(       Gimgio const * const img);
gimgioAPI GimgioType   gimgioGetTypePixel(      Gimgio const * const img);
gimgioAPI Gcount       gimgioGetWidth(          Gimgio const * const img);

gimgioAPI Gb           gimgioIsStarted(         void);

gimgioAPI Gb           gimgioLoad(              Gpath const * const filename, GimgioType const type, Gcount * const width, Gcount * const height, void ** const pixel);

gimgioAPI Gimgio      *gimgioOpen_(             Gpath const * const filename, GimgioOpenMode const mode, GimgioFormat const format);

gimgioAPI Gb           gimgioSetCompression(    Gimgio       * const img, Gr const amount);
gimgioAPI Gb           gimgioSetHeight(         Gimgio       * const img, Gcount const height);
gimgioAPI Gb           gimgioSetImageIndex(     Gimgio       * const img, Gindex const index);
gimgioAPI Gb           gimgioSetPixelRow(       Gimgio       * const img, void * const pixel);
gimgioAPI Gb           gimgioSetPixelAtN(       GimgioType const type, Gindex const index, void * const pixel, Gn4 const r, Gn4 const g, Gn4 const b, Gn4 const a);   
gimgioAPI Gb           gimgioSetPixelAtR(       GimgioType const type, Gindex const index, void * const pixel, Gr const r, Gr const g, Gr const b, Gr const a);   
gimgioAPI Gb           gimgioSetRow(            Gimgio       * const img, Gindex const index);
gimgioAPI Gb           gimgioSetTypeFile(       Gimgio       * const img, GimgioType const type);
gimgioAPI Gb           gimgioSetTypePixel(      Gimgio       * const img, GimgioType const type);
gimgioAPI Gb           gimgioSetWidth(          Gimgio       * const img, Gcount const width);

gimgioAPI Gb           gimgioStart(             void);
gimgioAPI void         gimgioStop(              void);

#define B1ToN4(V)   (((Gn4) V) << 31)
#define B2ToN4(V)   (((Gn4) V) << 30)
#define B4ToN4(V)   (((Gn4) V) << 28)
#define N1ToN4(V)   (((Gn4) V) << 24)
#define N2ToN4(V)   (((Gn4) V) << 16)
#define RToN4(V)    (((Gn4) gMIN(1., gMAX(0., V))) * GnMAX)

#define N4ToB1(V)   (((Gn4) V) >> 31)
#define N4ToB2(V)   (((Gn4) V) >> 30)
#define N4ToB4(V)   (((Gn4) V) >> 28)
#define N4ToN1(V)     (Gn1) (((Gn4) V) >> 24)
#define N4ToN2(V)     (Gn2) (((Gn4) V) >> 16)
#define N4ToR(V)    (((Gr) V) / (Gr) GnMAX)

/* C++ include */
#if defined(__cplusplus)
}
#endif
/***************/

#endif
