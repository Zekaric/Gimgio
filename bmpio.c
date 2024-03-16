/******************************************************************************

file:       bmpio.c
author:     Robbert de Groot

description:
bmp file handling.  Note, this is not entirely my code.  See copyright
below.  I did heavily alter the code to fit the scheme of Gimgio.

******************************************************************************/

/******************************************************************************
include: 
******************************************************************************/
#include "precompiled.h"

/******************************************************************************
local: 
type:
******************************************************************************/
// Worst case, file header size + v4 info header size + 256 4byte palette.
#define MAX_HEADER_SIZE (14 + 108 + 256 * 4)

#define headerSTART(D)           (D)->hptr = (D)->header;
#define headerSKIP(D, COUNT)     (D)->hptr += COUNT

#define headerGetN1(D, V) \
   V = *(D)->hptr; \
   (D)->hptr += 1

#define headerSetN1(D, V) \
   *((Gn1 *) ((void *) (D)->hptr)) = (Gn1) (V);\
   (D)->hptr += 1

#if grlSWAP_NEEDED == 1

#define headerGetN2(D, V)  \
   V = *((Gn2 *) ((void *) (D)->hptr));\
   gswap2(&(V));\
   (D)->hptr += 2
#define headerGetI2(D, V)  \
   V = *((Gi2 *) ((void *) (D)->hptr));\
   gswap2(&(V));\
   (D)->hptr += 2
#define headerGetN4(D, V)  \
   V = *((Gn4 *) ((void *) (D)->hptr));\
   gswap2(&(V));\
   (D)->hptr += 4
#define headerGetI4(D, V)  \
   V = *((Gi4 *) ((void *) (D)->hptr));\
   gswap2(&(V));\
   (D)->hptr += 4

#define headerSetN2(D, V)  \
   *((Gn2 *) ((void *) (D)->hptr)) = (Gn2) (V);\
   gswap2(((void *) (D)->hptr));\
   (D)->hptr += 2
#define headerSetI2(D, V)  \
   *((Gi2 *) ((void *) (D)->hptr)) = (Gi2) (V);\
   gswap2(((void *) (D)->hptr));\
   (D)->hptr += 2
#define headerSetN4(D, V)  \
   *((Gn4 *) ((void *) (D)->hptr)) = (Gn4) (V);\
   gswap2(((void *) (D)->hptr));\
   (D)->hptr += 4
#define headerSetI4(D, V)  \
   *((Gi4 *) ((void *) (D)->hptr)) = (Gi4) (V);\
   gswap2(((void *) (D)->hptr));\
   (D)->hptr += 4

#else

#define headerGetN2(D, V)  \
   V = *((Gn2 *) ((void *) (D)->hptr));\
   (D)->hptr += 2
#define headerGetI2(D, V)  \
   V = *((Gi2 *) ((void *) (D)->hptr));\
   (D)->hptr += 2
#define headerGetN4(D, V)  \
   V = *((Gn4 *) ((void *) (D)->hptr));\
   (D)->hptr += 4
#define headerGetI4(D, V)  \
   V = *((Gi4 *) ((void *) (D)->hptr));\
   (D)->hptr += 4

#define headerSetN2(D, V)  \
   *((Gn2 *) ((void *) (D)->hptr)) = (Gn2) (V);\
   (D)->hptr += 2
#define headerSetI2(D, V)  \
   *((Gi2 *) ((void *) (D)->hptr)) = (Gi2) (V);\
   (D)->hptr += 2
#define headerSetN4(D, V)  \
   *((Gn4 *) ((void *) (D)->hptr)) = (Gn4) (V);\
   (D)->hptr += 4
#define headerSetI4(D, V)  \
   *((Gi4 *) ((void *) (D)->hptr)) = (Gi4) (V);\
   (D)->hptr += 4

#endif

typedef enum
{
   bmpRenderingNONE,
   bmpRenderingERROR_DIFFUSION,
   bmpRenderingPANDA,
   bmpRenderingSUPER_CIRCLE_HALFTONING,
} BmpRendering;

typedef enum
{
   bmpColorSpaceNONE,
   bmpColorSpaceRGB        = 0,
   bmpColorSpaceRGB_DEVICE,
   bmpColorSpaceCMYK
} BmpColorSpace;

typedef enum
{
   bmpTypeNONE    = 0,
   bmpTypeBITMAP  = 0x4d42, // "BM
} BmpType;

typedef enum 
{
   bmpVersionNONE,
   bmpVersion2,
   bmpVersion2_OS2,
   bmpVersion3_WIN_3,
   bmpVersion3_WIN_NT,
   bmpVersion4
} BmpVersion;

typedef enum 
{
   bmpCompressionRAW       = 0,
   bmpCompressionRLE8      = 1,
   bmpCompressionRLE4      = 2,
   bmpCompressionBITFIELD  = 3,
   bmpCompressionHUFFMAN   = 3, // os2 file only. No information found. Not implemented
   bmpCompressionRLE24     = 4, // os2 file only.
} BmpCompression;

typedef struct 
{
   BmpType        ftype; 
   Gn4            fsize; 
   Gn4            fimageOffset; 

   Gn4            isize; 
   Gn2            iplanes; 
   Gn2            ibpp; 
   BmpCompression icompression; 
   Gn4            iimageSize; 
   Gi4            ixPelsPerMeter; 
   Gi4            iyPelsPerMeter; 
   Gn4            icolorUsed; 
   Gn4            icolorImportant; 
   Gn4            iisBottomUp;
   Gn4            iscanLineSize;
   BmpVersion     iversion;

   //v2 os2
   Gn2            iunits,
                  irecording;
   BmpRendering   irendering;
   Gn4            isize1,
                  isize2;
   Gn4            icolorEncoding;
   Gn4            iid;

   //v4
   BmpColorSpace  icolorSpace;
   Gi4            irx, iry, irz,
                  igx, igy, igz,
                  ibx, iby, ibz;
   Gn4            irGamma,
                  igGamma,
                  ibGamma;

   Gn4            rmask,
                  gmask,
                  bmask,
                  amask;

   Gn4            paletteCount;
   Gn1           *palette;

   Gn1            header[MAX_HEADER_SIZE];
   Gn1           *hptr;

   Gn1          **row;
} Bmpio;

/******************************************************************************
prototype:
******************************************************************************/
// callbacks.
static void _BmpDestroyContent(  Gimgio * const img);

static Gb   _BmpGetPixelRow(     Gimgio * const img, void * const pixel);

static Gb   _BmpReadStart(       Gimgio * const img);

static Gb   _BmpSetImageIndex(   Gimgio * const img, Gi4 const index);
static Gb   _BmpSetPixelRow(     Gimgio * const img, void * const pixel);
static Gb   _BmpSetTypeFile(     Gimgio * const img);

// completely local
static Gb   _CreateRowPointers(  Gimgio * const img, Bmpio * const data, Gi4 const rowSize);

static void _DestroyRowPointers( Gimgio * const img, Bmpio * const data);

static Gi4  _GetWidthPadded(     Gimgio * const img, Bmpio * const data);

static Gb   _ReadBmp(            Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmp1(           Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmp4(           Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmp8(           Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmp24(          Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmpRLE4(        Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmpRLE8(        Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmpRLE24(       Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmpBitField16(  Gimgio * const img, Bmpio * const data);
static Gb   _ReadBmpBitField32(  Gimgio * const img, Bmpio * const data);
static void _ReadBmpBitFieldInfo(Bmpio * const data, Gn4 const mask, Gn4 * const newMask, Gn4 * const offset);
static Gb   _ReadMask(           Gimgio * const img, Bmpio * const data);
static Gb   _ReadPalette(        Gimgio * const img, Bmpio * const data);

static Gb   _WriteBmp(           Gimgio * const img, Bmpio * const data);

/******************************************************************************
global: to library only
function: 
******************************************************************************/
/******************************************************************************
func: bmpioCreateContent

Create the content for handling the bmp file.
******************************************************************************/
Gb bmpioCreateContent(Gimgio * const img)
{
   Bmpio *data;

   genter;

   data = gmemCreateType(Bmpio);
   greturnFalseIf(!data);

   img->data           = data;
   img->DestroyContent = _BmpDestroyContent;
   img->GetPixelRow    = _BmpGetPixelRow;
   img->ReadStart      = _BmpReadStart;
   img->SetImageIndex  = _BmpSetImageIndex;
   img->SetPixelRow    = _BmpSetPixelRow;
   img->SetTypeFile    = _BmpSetTypeFile;

   greturn gbTRUE;
}

/******************************************************************************
local: 
function:
******************************************************************************/
/******************************************************************************
func: _BmpDestroyContent

Clean up.
******************************************************************************/
static void _BmpDestroyContent(Gimgio * const img)
{
   Bmpio *data;

   genter;

   data = (Bmpio *) img->data;

   // Write out the image
   if (img->mode == gimgioOpenWRITE)
   {
      _WriteBmp(img, data);
   }

   _DestroyRowPointers(img, data);
   
   gmemDestroy(data->palette);

   gmemDestroy(img->data);
   img->data = NULL;

   greturn;
}

/******************************************************************************
func: _BmpGetPixelRow

Read in a pixel row.
******************************************************************************/
static Gb _BmpGetPixelRow(Gimgio * const img, void * const pixel)
{
   Bmpio *data;

   genter;

   data = (Bmpio *) img->data;

   if (!data->row)
   {
      greturnFalseIf(!_ReadBmp(img, data));
   }

   // Convert the pixel row to what we want.
   gimgioConvert(
      img->width,
      img->typeFile,
      data->row[img->row],
      img->typePixel,
      pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _BmpReadStart

Read in the image information.
******************************************************************************/
static Gb _BmpReadStart(Gimgio * const img)
{
   Bmpio *data;
   Gi4    i4temp;

   genter;

   data = (Bmpio *) img->data;

   // Read in all the header information in one read.
   gfileGet(img->file, MAX_HEADER_SIZE, data->header);

   headerSTART(data);

   headerGetN2(data, data->ftype);

   // Read in the file header for the BMP file.
   // Currently only handle BM BMP files.
   // v2+ file. "BM"
   greturnFalseIf(data->ftype != bmpTypeBITMAP);
   
   headerGetN4(data, data->fsize);

   headerSKIP(data, 4);
   
   headerGetN4(data, data->fimageOffset);

   // Read in the info header information.
   headerGetN4(data, data->isize);

   // v2 file.
   if      (data->isize == 12)
   {
      data->iversion = bmpVersion2;

      headerGetI2(data, img->width);
      headerGetI2(data, img->height);

      headerGetN2(data, data->iplanes);
      headerGetN2(data, data->ibpp);

      // Is there a palette?  14 file header size, 12 bitmap header size, 
      // palette entry size.
      data->paletteCount = (data->fimageOffset - 14 - 12) / 3;
   }
   // v3 file.
   else 
   {
      if      (data->isize == 108)
      {
         data->iversion = bmpVersion4;
      }
      else if (data->isize == 64)
      {
         data->iversion = bmpVersion2_OS2;
      }
      else
      {
         data->iversion = bmpVersion3_WIN_3;
      }

      headerGetI4(data, img->width);
      headerGetI4(data, i4temp);
      data->iisBottomUp = (i4temp > 0);
      img->height       = gABS(i4temp);

      headerGetN2(data, data->iplanes);
      headerGetN2(data, data->ibpp);
      headerGetN4(data, data->icompression);
      headerGetN4(data, data->iimageSize);
      headerGetI4(data, data->ixPelsPerMeter);
      headerGetI4(data, data->iyPelsPerMeter);
      headerGetN4(data, data->icolorUsed);
      headerGetN4(data, data->icolorImportant);

      if (data->iversion     == bmpVersion3_WIN_3 &&
          data->icompression == bmpCompressionBITFIELD)
      {
         data->iversion = bmpVersion3_WIN_NT;
      }

      if (data->iversion == bmpVersion2_OS2)
      {
         headerGetN2(data, data->iunits);
         headerSKIP(data, 2);
         headerGetN2(data, data->irecording);
         headerGetN2(data, data->irendering);
         headerGetN4(data, data->isize1);
         headerGetN4(data, data->isize2);
         headerGetN4(data, data->icolorEncoding);
         headerGetN4(data, data->iid);
      }

      data->paletteCount = data->icolorUsed;

      greturnFalseIf(!_ReadMask(img, data));

      if (data->iversion == bmpVersion4)
      {
         headerGetN4(data, data->icolorSpace);
         headerGetI4(data, data->irx);
         headerGetI4(data, data->iry);
         headerGetI4(data, data->irz);
         headerGetI4(data, data->igx);
         headerGetI4(data, data->igy);
         headerGetI4(data, data->igz);
         headerGetI4(data, data->ibx);
         headerGetI4(data, data->iby);
         headerGetI4(data, data->ibz);
         headerGetN4(data, data->irGamma);
         headerGetN4(data, data->igGamma);
         headerGetN4(data, data->ibGamma);
      }
   }

   greturnFalseIf(!_ReadPalette(img, data));

   greturn gbTRUE;
}

/******************************************************************************
func: _BmpSetImageIndex

Bmp does not support multiple images.
******************************************************************************/
static Gb _BmpSetImageIndex(Gimgio * const img, Gi4 const index)
{
   genter;
   img; index;
   greturn gbFALSE;
}

/******************************************************************************
func: _BmpSetPixelRow

Set the pixels of a row.
******************************************************************************/
static Gb _BmpSetPixelRow(Gimgio * const img, void * const pixel)
{
   Bmpio *data;

   genter;

   data = (Bmpio *) img->data;

   if (!data->row)
   {
      greturnFalseIf(
         !_CreateRowPointers(
            img,
            data, 
            gimgioGetPixelSize(img->typeFile, img->width)));
   }

   gimgioConvert(
      img->width, 
      img->typePixel,
      pixel,
      img->typeFile,
      data->row[img->row]);

   greturn gbTRUE;
}

/******************************************************************************
func: _BmpSetTypeFile

Ensure the given type is allowed.  If not default to something valid.
******************************************************************************/
static Gb _BmpSetTypeFile(Gimgio * const img)
{
   genter;

   greturnTrueIf(img->typeFile == (gimgioTypeRGB | gimgioTypeN1));
   
   // Default to RGB N1
   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   greturn gbFALSE;
}

/******************************************************************************
func: _CreateRowPointers

Create the row pointers.
******************************************************************************/
static Gb _CreateRowPointers(Gimgio * const img, Bmpio * const data, Gi4 const rowSize)
{
   Gi4 a;

   genter;

   // Allocate the 'row pointers', image buffer.
   data->row = gmemCreateTypeArray(Gn1 *, img->height);
   greturnFalseIf(!data->row);

   // Allocate the rows.
   forCount(a, img->height)
   {
      data->row[a] = gmemCreateTypeArray(Gn1, rowSize);
      greturnFalseIf(!data->row[a]);
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _DestroyRowPointers

Destroy the row pointers.
******************************************************************************/
static void _DestroyRowPointers(Gimgio * const img, Bmpio * const data)
{
   Gi4 a;

   genter;

   if (data->row)
   {
      forCount(a, img->height)
      {
         gmemDestroy(data->row[a]);
      }

      gmemDestroy(data->row);
   }
   data->row = NULL;

   greturn;
}

/******************************************************************************
func: _GetWidthPadded

Get the byte count of a row and find the padded to 4 byte boundary width.
******************************************************************************/
static Gi4 _GetWidthPadded(Gimgio * const img, Bmpio * const data)
{
   Gi4 widthWithPad;
   
   genter;

   widthWithPad = img->width * data->ibpp / 8;
   switch(widthWithPad % 4)
   {
   case 1: widthWithPad++;
   case 2: widthWithPad++;
   case 3: widthWithPad++;
   }

   greturn widthWithPad;
}

/******************************************************************************
func: _ReadBmp

Read in the image if not read already.
******************************************************************************/
static Gb _ReadBmp(Gimgio * const img, Bmpio * const data)
{
   Gb result;
   
   genter;

   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   if (!data->row)
   {
      greturnFalseIf(
         !_CreateRowPointers(
            img, 
            data, 
            gimgioGetPixelSize(img->typeFile, img->width)));
   }

   gfileSetPosition(img->file, gpositionSTART, data->fimageOffset);

   switch(data->ibpp)
   {
   case 1:
      result = _ReadBmp1(img, data);

      greturn result;

   case 4:
      switch(data->icompression)
      {
      case bmpCompressionRAW:
         result = _ReadBmp4(img, data);

         greturn result;

      case bmpCompressionRLE4:
         result = _ReadBmpRLE4(img, data);

         greturn result;
      }
      break;

   case 8:
      switch(data->icompression)
      {
      case bmpCompressionRAW:
         result = _ReadBmp8(img, data);

         greturn result;

      case bmpCompressionRLE8:
         result = _ReadBmpRLE8(img, data);

         greturn result;
      }
      break;

   case 16:
      result = _ReadBmpBitField16(img, data);

      greturn result;

   case 24:
      switch(data->icompression)
      {
      case bmpCompressionRAW:
         result = _ReadBmp24(img, data);

         greturn result;

      case bmpCompressionRLE24: // os2 file only
         result = _ReadBmpRLE24(img, data);

         greturn result;
      }
      break;

   case 32:
      result = _ReadBmpBitField32(img, data);

      greturn result;
   }

   greturn gbFALSE;
}

/******************************************************************************
func: _ReadBmp1

Read in a 2 color bitmap.
******************************************************************************/
static Gb _ReadBmp1(Gimgio * const img, Bmpio * const data)
{
   Gi4  widthWithPad,
        rindex,
        cindex,
        byte,
        row,
        bit;
   Gn1 *pixel; 

   genter;

   widthWithPad = _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn1, widthWithPad);
   greturnFalseIf(!pixel);

   forCount(rindex, img->height)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = img->height - 1 - rindex;
      }

      forCount(cindex, img->width)
      {
         byte = cindex / 8;
         bit  = ((pixel[byte] & (1 << (7 - (cindex % 8)))) == 0) ? 0 : 1;

         data->row[row][cindex * 3 + 0] = data->palette[bit * 4 + 0];
         data->row[row][cindex * 3 + 1] = data->palette[bit * 4 + 1];
         data->row[row][cindex * 3 + 2] = data->palette[bit * 4 + 2];
      }
   }

   // Clean up
   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmp4

Read in the 16 color image.
******************************************************************************/
static Gb _ReadBmp4(Gimgio * const img, Bmpio * const data)
{
   Gi4  widthWithPad,
        rindex,
        cindex,
        byte,
        row,
        quadBit;
   Gn1 *pixel; 

   genter;

   widthWithPad = _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn1, widthWithPad);
   greturnFalseIf(!pixel);

   forCount(rindex, img->height)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = img->height - 1 - rindex;
      }

      forCount(cindex, img->width)
      {
         byte = cindex / 2;
         if ((cindex % 2) == 0)
         {
            quadBit = (pixel[byte] >> 4) & 0xf;
         }
         else
         {
            quadBit = pixel[byte] & 0xf;
         }

         data->row[row][cindex * 3 + 0] = data->palette[quadBit * 4 + 0];
         data->row[row][cindex * 3 + 1] = data->palette[quadBit * 4 + 1];
         data->row[row][cindex * 3 + 2] = data->palette[quadBit * 4 + 2];
      }
   }

   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmp8

Read in a 256 color image.
******************************************************************************/
static Gb _ReadBmp8(Gimgio * const img, Bmpio * const data)
{
   Gi4  widthWithPad,
        rindex,
        cindex,
        row;
   Gn1 *pixel,
        byte; 

   genter;

   widthWithPad = _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn1, widthWithPad);
   greturnFalseIf(!pixel);

   forCount(rindex, img->height)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = img->height - 1 - rindex;
      }

      forCount(cindex, img->width)
      {
         byte = pixel[cindex];
         data->row[row][cindex * 3 + 0] = data->palette[byte * 4 + 0];
         data->row[row][cindex * 3 + 1] = data->palette[byte * 4 + 1];
         data->row[row][cindex * 3 + 2] = data->palette[byte * 4 + 2];
      }
   }

   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmp24

Read in a 24 bit image.
******************************************************************************/
static Gb _ReadBmp24(Gimgio * const img, Bmpio * const data)
{
   Gi4  widthWithPad,
        rindex,
        cindex,
        row;
   Gn1 *pixel; 

   genter;

   widthWithPad = _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn1, widthWithPad);
   greturnFalseIf(!pixel);

   forCount(rindex, img->height)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = img->height - 1 - rindex;
      }

      forCount(cindex, img->width)
      {
         // BGR to RGB
         data->row[row][cindex * 3 + 0] = pixel[cindex * 3 + 2];
         data->row[row][cindex * 3 + 1] = pixel[cindex * 3 + 1];
         data->row[row][cindex * 3 + 2] = pixel[cindex * 3 + 0];
      }
   }

   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpRLE4

Read in a RLE compressed 16 color image.
******************************************************************************/
static Gb _ReadBmpRLE4(Gimgio * const img, Bmpio * const data)
{
   Gindex loopIndex,
          rindex,
          cindex,
          row,
          byteIndex,
          runIndex;
   Gcount count;
   Gi4    quadBit;
   Gn1    byte[256]; 

   genter;

   rindex = 0;

   row = rindex;
   if (data->iisBottomUp)
   {
      row = img->height - 1 - rindex;
   }

   cindex = 0;

   loopCount(loopIndex)
   {
      // Read RLE Header and follow byte.
      gfileGet(img->file, 2, byte);

      // Repeat run
      if      (byte[0] > 0)
      {
         count = byte[0];

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            byteIndex = runIndex % 2;
            if (byteIndex == 0)
            {
               quadBit = (byte[1] >> 4) & 0xf;
            }
            else
            {
               quadBit = byte[1] & 0xf;
            }

            data->row[row][cindex * 3 + 0] = data->palette[quadBit * 4 + 0];
            data->row[row][cindex * 3 + 1] = data->palette[quadBit * 4 + 1];
            data->row[row][cindex * 3 + 2] = data->palette[quadBit * 4 + 2];
            cindex++;

            breakIf(cindex >= img->width);
         }
      }
      // Raw run
      else if (byte[1] >= 3)
      {
         count = byte[1];
         gfileGet(
            img->file, 
            ((count / 2) & 1) ? (count / 2) + 1 : (count / 2),
            byte);

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            byteIndex = runIndex / 2;
            if ((runIndex % 2) == 0)
            {
               quadBit = (byte[byteIndex] >> 4) & 0xf;
            }
            else
            {
               quadBit = byte[byteIndex] & 0xf;
            }

            data->row[row][cindex * 3 + 0] = data->palette[quadBit * 4 + 0];
            data->row[row][cindex * 3 + 1] = data->palette[quadBit * 4 + 1];
            data->row[row][cindex * 3 + 2] = data->palette[quadBit * 4 + 2];
            cindex++;

            breakIf(cindex >= img->width);
         }
      }
      // End of scan line.
      else if (byte[1] == 0)
      {
         // reset the column index
         cindex = 0;

         rindex++;
         row = rindex;
         if (data->iisBottomUp)
         {
            row = img->height - 1 - rindex;
         }
      }
      // End of RLE data
      else if (byte[1] == 1)
      {
         break;
      }
      // Move the current pixel.
      else if (byte[1] == 2)
      {
         gfileGet(img->file, 2, byte);
         cindex += byte[0];
         rindex += byte[1];
      }
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpRLE8

Read in an RLE 8 compressed 256 color image.
******************************************************************************/
static Gb _ReadBmpRLE8(Gimgio * const img, Bmpio * const data)
{
   Gindex loopIndex,
          rindex,
          cindex,
          row,
          byteIndex,
          runIndex;
   Gcount count;
   Gn1    byte[256]; 

   genter;

   rindex = 0;

   row = rindex;
   if (data->iisBottomUp)
   {
      row = img->height - 1 - rindex;
   }

   cindex = 0;

   loopCount(loopIndex)
   {
      // Read RLE Header and follow byte.
      gfileGet(img->file, 2, byte);

      // Repeat run
      if      (byte[0] > 0)
      {
         count     = byte[0];
         byteIndex = byte[1];

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            data->row[row][cindex * 3 + 0] = data->palette[byteIndex * 4 + 0];
            data->row[row][cindex * 3 + 1] = data->palette[byteIndex * 4 + 1];
            data->row[row][cindex * 3 + 2] = data->palette[byteIndex * 4 + 2];
            cindex++;
            breakIf(cindex >= img->width);
         }
      }
      // Raw run
      else if (byte[1] >= 3)
      {
         count = byte[1];
         gfileGet(
            img->file, 
            (count & 1) ? count + 1 : count, 
            byte);

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            data->row[row][cindex * 3 + 0] = data->palette[byte[runIndex] * 4 + 0];
            data->row[row][cindex * 3 + 1] = data->palette[byte[runIndex] * 4 + 1];
            data->row[row][cindex * 3 + 2] = data->palette[byte[runIndex] * 4 + 2];
            cindex++;
            breakIf(cindex >= img->width);
         }
      }
      // End of scan line.
      else if (byte[1] == 0)
      {
         // reset the column index
         cindex = 0;

         rindex++;
         row = rindex;
         if (data->iisBottomUp)
         {
            row = img->height - 1 - rindex;
         }
      }
      // End of RLE data
      else if (byte[1] == 1)
      {
         break;
      }
      // Move the current pixel.
      else if (byte[1] == 2)
      {
         gfileGet(img->file, 2, byte);
         cindex += byte[0];
         rindex += byte[1];
      }
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpRLE24

Read in an OS2 RLE 24 compressed 24 bit image.
******************************************************************************/
static Gb _ReadBmpRLE24(Gimgio * const img, Bmpio * const data)
{
   Gindex loopIndex,
          rindex,
          cindex,
          row,
          runIndex;
   Gcount count;
   Gn1    byte[256 * 3]; 

   genter;

   rindex = 0;

   row = rindex;
   if (data->iisBottomUp)
   {
      row = img->height - 1 - rindex;
   }

   cindex = 0;

   loopCount(loopIndex)
   {
      // Read RLE Header and follow byte.
      gfileGet(img->file, 1, byte);

      // Repeat run
      if      (byte[0] > 0)
      {
         count = byte[0];
         gfileGet(img->file, 3, byte);

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            data->row[row][cindex * 3 + 0] = byte[2];
            data->row[row][cindex * 3 + 1] = byte[1];
            data->row[row][cindex * 3 + 2] = byte[0];
            cindex++;
         }

         continue;
      }

      gfileGet(img->file, 1, byte);

      // Raw run
      if      (byte[0] >= 3)
      {
         count = byte[0];
         gfileGet(img->file, count * 3, byte);

         for (runIndex = 0; runIndex < count; runIndex++)
         {
            data->row[row][cindex * 3 + 0] = byte[runIndex * 3 + 2];
            data->row[row][cindex * 3 + 1] = byte[runIndex * 3 + 1];
            data->row[row][cindex * 3 + 2] = byte[runIndex * 3 + 0];
            cindex++;
         }
      }
      // End of scan line.
      else if (byte[0] == 0)
      {
         // reset the column index
         cindex = 0;

         rindex++;
         row = rindex;
         if (data->iisBottomUp)
         {
            row = img->height - 1 - rindex;
         }
      }
      // End of RLE data
      else if (byte[0] == 1)
      {
         break;
      }
      // Move the current pixel.
      else if (byte[0] == 2)
      {
         gfileGet(img->file, 2, byte);
         cindex += byte[0];
         rindex += byte[1];
      }
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpBitField16

Read in the image using the bit fields.
******************************************************************************/
static Gb _ReadBmpBitField16(Gimgio * const img, Bmpio * const data)
{
   Gi4  rindex,
        cindex,
        row;
   Gn4  widthWithPad,
        rmask,
        gmask,
        bmask,
        amask,
        roff,
        goff,
        boff,
        aoff;
   Gn2 *pixel;

   genter;

   widthWithPad = (Gn4) _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn2, widthWithPad / 2);
   greturnFalseIf(!pixel);

   // Get the manipulations.
   _ReadBmpBitFieldInfo(data, data->amask, &amask, &aoff);
   _ReadBmpBitFieldInfo(data, data->bmask, &bmask, &boff);
   _ReadBmpBitFieldInfo(data, data->gmask, &gmask, &goff);
   _ReadBmpBitFieldInfo(data, data->rmask, &rmask, &roff);

   for (rindex = 0; rindex < img->height; rindex++)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = (Gn4) (img->height - 1 - rindex);
      }

      for (cindex = 0; cindex < img->width; cindex++)
      {
         data->row[row][cindex * 3 + 0] = (Gn1) ((pixel[cindex] >> roff) & rmask);
         data->row[row][cindex * 3 + 1] = (Gn1) ((pixel[cindex] >> goff) & gmask);
         data->row[row][cindex * 3 + 2] = (Gn1) ((pixel[cindex] >> boff) & bmask);
         // a ignored for now.
      }
   }

   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpBitField32

Read in the image using the bit fields.
******************************************************************************/
static Gb _ReadBmpBitField32(Gimgio * const img, Bmpio * const data)
{
   Gi4  rindex,
        cindex,
        row;
   Gn4  widthWithPad,
        rmask,
        gmask,
        bmask,
        amask,
        roff,
        goff,
        boff,
        aoff,
       *pixel;

   genter;

   widthWithPad = (Gn4) _GetWidthPadded(img, data);
   pixel        = gmemCreateTypeArray(Gn4, widthWithPad / 4);
   greturnFalseIf(!pixel);

   // Get the manipulations.
   _ReadBmpBitFieldInfo(data, data->amask, &amask, &aoff);
   _ReadBmpBitFieldInfo(data, data->bmask, &bmask, &boff);
   _ReadBmpBitFieldInfo(data, data->gmask, &gmask, &goff);
   _ReadBmpBitFieldInfo(data, data->rmask, &rmask, &roff);

   for (rindex = 0; rindex < img->height; rindex++)
   {
      gfileGet(img->file, widthWithPad, pixel);

      row = rindex;
      if (data->iisBottomUp)
      {
         row = (Gn4) (img->height - 1 - rindex);
      }

      for (cindex = 0; cindex < img->width; cindex++)
      {
         data->row[row][cindex * 3 + 0] = (Gn1) ((pixel[cindex] >> roff) & rmask);
         data->row[row][cindex * 3 + 1] = (Gn1) ((pixel[cindex] >> goff) & gmask);
         data->row[row][cindex * 3 + 2] = (Gn1) ((pixel[cindex] >> boff) & bmask);
         // a ignored for now.
      }
   }

   gmemDestroy(pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadBmpBitFieldInfo

Get the shifted down mask and the amount to shift the bits.
******************************************************************************/
static void _ReadBmpBitFieldInfo(Bmpio * const data, Gn4 const mask, Gn4 * const newMask, 
   Gn4 * const offset)
{
   Gn4 m,
       o;

   genter;

   *newMask = 0;
   *offset  = 0;
   greturnVoidIf(!mask);
   
   // most significant bits used.  only 16 of the 32 are used for 16 bpp 
   // images.
   m = 1;
   if (data->ibpp == 16)
   {
      m = mask >> 16;
   }

   // Find the number of shifts to drop the mask down.
   for (o = 0; ; o++)
   {
      breakIf(m & 1);
      m = m >> 1;
   }

   *newMask = m;
   *offset  = o;

   greturn;
}

/******************************************************************************
func: _ReadMask

Read in the masks.
******************************************************************************/
static Gb _ReadMask(Gimgio * const img, Bmpio * const data)
{
   genter;

   img;

   if (data->iversion != bmpVersion4)
   {
      greturnFalseIf(data->icompression != bmpCompressionBITFIELD);
   }

   headerGetN4(data, data->rmask);
   headerGetN4(data, data->gmask);
   headerGetN4(data, data->bmask);

   if (data->iversion == bmpVersion4)
   {
      headerGetN4(data, data->amask);
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _ReadPalette

Read in the palette.
******************************************************************************/
static Gb _ReadPalette(Gimgio * const img, Bmpio * const data)
{
   Gn4 index,
       size;

   genter;

   img;

   greturnTrueIf(!data->paletteCount);

   data->palette = gmemCreateTypeArray(Gn1, data->paletteCount * 4);
   greturnFalseIf(!data->palette);

   if (data->iversion == bmpVersion2 ||
       data->iversion == bmpVersion2_OS2)
   {
      size = 3;
   }
   else
   {
      forCount(index, data->paletteCount)
      {
         // BGR order.  Read in as RGB.
         headerGetN1(data, data->palette[index * 4 + 2]);
         headerGetN1(data, data->palette[index * 4 + 1]);
         headerGetN1(data, data->palette[index * 4 + 0]);

         data->palette[index * 4 + 3] = 0xff;

         if (data->iversion != bmpVersion2)
         {
            // skipped
            headerSKIP(data, 1);
         }
      }
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _WriteBmp

Write out the bmp file.
******************************************************************************/
static Gb _WriteBmp(Gimgio * const img, Bmpio * const data) 
{
   Gi4   row,
         col,
         widthWithPad;
   Gn1  *pixel;

   genter;

   // Get the padded width.  Each scan line ends on a 4 byte boundary.
   data->ibpp   = 24;
   widthWithPad = _GetWidthPadded(img, data);

   pixel = gmemCreateTypeArray(Gn1, widthWithPad);
   greturnFalseIf(!pixel);

   // Write out a plain old raw 24 bit image.
   // Read in all the header information in one read.
   headerSTART(data);

   // 
   // File Header
   //
   headerSetN2(data, bmpTypeBITMAP);
   // file header size + v3 info header size + image size.
   headerSetN4(data, 14 + 40 + widthWithPad * img->height * 3);
   headerSKIP(data, 4);
   // file header size + v3 info header size
   headerSetN4(data, 14 + 40);

   //
   // Info Header
   //
   headerSetN4(data, 40);
   headerSetI4(data, img->width);
   headerSetI4(data, img->height);
   headerSetN2(data, 1);
   headerSetN2(data, 24);
   headerSetN4(data, bmpCompressionRAW);
   headerSetN4(data, 0);
   headerSetI4(data, 1000);
   headerSetI4(data, 1000);
   headerSetN4(data, 0);
   headerSetN4(data, 0);

   // Write out the headers
   gfileSet(
      img->file, 
      14 + 40,
      data->header,
      NULL);

   // Write out the image data.
   forCountDown(row, img->height)
   {
      forCount(col, img->width)
      {
         // bgr
         pixel[col * 3 + 0] = data->row[row][col * 3 + 2];
         pixel[col * 3 + 1] = data->row[row][col * 3 + 1];
         pixel[col * 3 + 2] = data->row[row][col * 3 + 0];
      }
      gfileSet(img->file, widthWithPad, pixel, NULL);
   }

   // Clean up
   gmemDestroy(pixel);

   greturn gbTRUE;
}
