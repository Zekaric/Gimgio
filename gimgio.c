/******************************************************************************

file:       gimgio.c
author:     Robbert de Groot
copyright:  2008-2008, Robbert de Groot

description:
Image IO routines.  Handles the standard image formats like png, jpeg,
bmp.  This will never be as complete as LEADTOOLs or image magic.  This
is meant only to handle the 'major' formats and leave the rest.

******************************************************************************/

/******************************************************************************
include: 
******************************************************************************/
#include "precompiled.h"

//#include "tiffiop.h"

#define gimgioLOCAL_INCLUDE


/******************************************************************************
local: 
type:
******************************************************************************/

/******************************************************************************
prototype:
******************************************************************************/
static Gb _Start(Gimgio * const img);

/******************************************************************************
global:
function: 
******************************************************************************/
/******************************************************************************
func: gimgioClose

Clean up.
******************************************************************************/
gimgioAPI void gimgioClose(Gimgio * const img)
{
   genter;

   greturnVoidIf(!img);

   // Clean up.
   if (img->DestroyContent)
   {
      img->DestroyContent(img);
   }

#if defined(GIMGIO_TIFF)
   // close the file.
   if (img->tiffFile)
   {
      TIFFClose(img->tiffFile);
   }
#endif
   gfileClose(img->file);

   gmemDestroy(img);

   greturn;
}

/******************************************************************************
func: gimgioConvert

Convert a row from one format to another.
******************************************************************************/
gimgioAPI void gimgioConvert(Gi4 const width, GimgioType const inType, void * const in,
   GimgioType const outType, void * const out)
{
   Gi4 index;

   if (inType & gimgioTypeREAL)
   {
      Gr r, g, b, a;

      forCount(index, width)
      {
         gimgioGetPixelAtR(inType,  index, in,  &r, &g, &b, &a);
         gimgioSetPixelAtR(outType, index, out, r,  g,  b,  a);
      }
   }
   else
   {
      Gn4 r, g, b, a;

      forCount(index, width)
      {
         gimgioGetPixelAtN(inType,  index, in,  &r, &g, &b, &a);
         gimgioSetPixelAtN(outType, index, out, r,  g,  b,  a);
      }
   }
}

/******************************************************************************
func: gimgioGetCompression

Get the compression amount.
******************************************************************************/
gimgioAPI Gr gimgioGetCompression(Gimgio const * const img)
{
   genter;

   greturnIf(!img, 0.);

   greturn (Gr4) (img->compression * 100.);
}

/******************************************************************************
func: gimgioGetFormat

Get the file format.
******************************************************************************/
gimgioAPI GimgioFormat gimgioGetFormat(Gimgio const * const img)
{
   genter;

   greturnIf(!img, gimgioFormatNONE);

   greturn img->format;
}

/******************************************************************************
func: gimgioGetFormatFromName

Get the format from the extension
******************************************************************************/
gimgioAPI GimgioFormat gimgioGetFormatFromName(Gpath const * const path)
{
   Gs          *extension,
               *fileName;
   GimgioFormat result;

   genter;

   fileName = gpathGetEnd(path);

   result    = gimgioFormatNONE;
   extension = gsCreateFromSub(
      fileName, 
      gsFindLastOfA(fileName, 0, ".") + 1,
      gsGetCount(fileName));

   // try to figure the format from the extension.  Override the 
   // provided format if set.
   if      (gsCompareBaseA(extension, "BMP") == gcompareEQUAL)
   {
      result = gimgioFormatBMP;
   }
   else if (gsCompareBaseA(extension, "GIF") == gcompareEQUAL)
   {
      result = gimgioFormatGIF;
   }
   else if (gsCompareBaseA(extension, "JPG")  == gcompareEQUAL ||
            gsCompareBaseA(extension, "JPEG") == gcompareEQUAL)
   {
      result = gimgioFormatJPG;
   }
   else if (gsCompareBaseA(extension, "PNG") == gcompareEQUAL)
   {
      result = gimgioFormatPNG;
   }
   //else if (gsCompareBaseA(extension, "PPM") == gcompareEQUAL)
   //{
   //   result = gimgioFormatPPM;
   //}
   else if (gsCompareBaseA(extension, "GRAW") == gcompareEQUAL)
   {
      result = gimgioFormatGRAW;
   }
   //else if (gsCompareBaseA(extension, "RLE") == gcompareEQUAL)
   //{
   //   result = gimgioFormatRLE;
   //}
   //else if (gsCompareBaseA(extension, "TRG") == gcompareEQUAL)
   //{
   //   result = gimgioFormatTRG;
   //}
   else if (gsCompareBaseA(extension, "TIF")  == gcompareEQUAL ||
            gsCompareBaseA(extension, "TIFF") == gcompareEQUAL)
   {
      result = gimgioFormatTIFF;
   }

   // Read in the basic information of the file.
   gsDestroy(extension);
   gsDestroy(fileName);

   greturn result;
}

/******************************************************************************
func: gimgioGetHeight

Get the height of the current image.
******************************************************************************/
gimgioAPI Gi4 gimgioGetHeight(Gimgio const * const img)
{
   genter;

   greturnIf(!img, 0);

   greturn img->height;
}

/******************************************************************************
func: gimgioGetImageCount

Get the number of images in the file.
******************************************************************************/
gimgioAPI Gi4 gimgioGetImageCount(Gimgio const * const img)
{
   genter;

   greturnIf(img, 0);

   greturn img->imageCount;
}

/******************************************************************************
func: gimgioGetImageIndex

Get the current image index
******************************************************************************/
gimgioAPI Gi4 gimgioGetImageIndex(Gimgio const * const img)
{
   genter;
   
   greturnIf(!img, 0);

   greturn img->imageIndex;
}

/******************************************************************************
func: gimgioGetPixelRow

Get the pixel row.  pixel provided should be large enough for one 
row of pixels.
******************************************************************************/
gimgioAPI Gb gimgioGetPixelRow(Gimgio * const img, void * const pixel)
{
   Gb result;
   
   genter;

   greturnFalseIf(!img);

   // read the row information.
   result = img->GetPixelRow(img, pixel);

   greturn result;
}

/******************************************************************************
func: gimgioGetPixelRowAll

Do the heavy lifting on loading the image.
******************************************************************************/
gimgioAPI Gb gimgioGetPixelRowAll(Gimgio * const img, Gn1 * const pixel)
{
   Gi4 row;

   // Read in the entire image.
   forCount(row, img->height)
   {
      gimgioSetRow(img, row);

      gimgioGetPixelRow(img, &(pixel[gimgioGetPixelSize(img->typePixel, img->width) * row]));
   }

   return gbTRUE;
}

/******************************************************************************
func: gimgioGetPixelSize

Get the size of row of pixels.
******************************************************************************/
gimgioAPI Gi4 gimgioGetPixelSize(GimgioType const type, Gi4 const width)
{
   switch (type)
   {
   case gimgioTypeBLACK | gimgioTypeB1:                     return (width + 7) / 8;
   case gimgioTypeBLACK | gimgioTypeB2:                     return (width + 3) / 4;
   case gimgioTypeBLACK | gimgioTypeB4:                     return (width + 1) / 2;
   case gimgioTypeBLACK | gimgioTypeN1:                     return  width;
   case gimgioTypeBLACK | gimgioTypeN2:                     return  width * 2;
   case gimgioTypeBLACK | gimgioTypeN4:    
   case gimgioTypeBLACK | gimgioTypeR4:                     return  width * 4;
   case gimgioTypeBLACK | gimgioTypeR8:                     return  width * 8;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB1:   return (width + 3) / 4;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB2:   return (width + 1) / 2;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB4:   return  width;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1:   return  width * 2;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2:   return  width * 4;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN4:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR4:   return  width * 8;
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR8:   return  width * 16;
   case gimgioTypeRGB | gimgioTypeB1:                       return (width + 1) / 2;
   case gimgioTypeRGB | gimgioTypeB2:                       return  width;
   case gimgioTypeRGB | gimgioTypeB4:                       return  width * 2;
   case gimgioTypeRGB | gimgioTypeN1:                       return  width * 3;
   case gimgioTypeRGB | gimgioTypeN2:                       return  width * 6;
   case gimgioTypeRGB | gimgioTypeN4:  
   case gimgioTypeRGB | gimgioTypeR4:                       return  width * 12;
   case gimgioTypeRGB | gimgioTypeR8:                       return  width * 24;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB1:     return (width + 1) / 2;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB2:     return  width;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB4:     return  width * 2;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1:     return  width * 4;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2:     return  width * 8;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN4:
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR4:     return  width * 16;
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR8:     return  width * 32;
   }

   return 0;
}

/******************************************************************************
func: gimgioGetPixelAtN

Get the n'th pixel value.
******************************************************************************/
gimgioAPI void gimgioGetPixelAtN(GimgioType const type, Gi4 const index, void * const pixel,
   Gn4 * const r, Gn4 * const g, Gn4 * const b, Gn4 * const a)
{
   Gn1 *buffer;
   Gn2 *n2;
   Gn4 *n4;
   Gr4 *r4;
   Gr8 *r8;

   buffer = (Gn1 *) pixel;

   switch(type)
   {
   case gimgioTypeBLACK | gimgioTypeB1:
      *r    = 
         *g = 
         *b = B1ToN4(gbitGet(buffer[index / 8], index % 8, 1));
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeB2:
      *r    = 
         *g = 
         *b = B2ToN4(gbitGet(buffer[index / 4], (index % 4) * 2, 2));
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeB4:
      *r    = 
         *g = 
         *b = B4ToN4(gbitGet(buffer[index / 2], (index % 2) * 4, 4));
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeN1:
      *r    = 
         *g = 
         *b = N1ToN4(buffer[index]);
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 2]);
      *r    = 
         *g = 
         *b = N2ToN4(*n2);
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeN4:
      n4 = (Gn4 *) &(buffer[index * 4]);
      *r    = 
         *g = 
         *b = *n4;
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 4]);
      *r    = 
         *g = 
         *b = RToN4(*r4);
      *a    = Gn4MAX;
      break;

   case gimgioTypeBLACK | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 8]);
      *r    = 
         *g = 
         *b = RToN4(*r8);
      *a    = Gn4MAX;
      break;



   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB1:
      *r    = 
         *g = 
         *b = B1ToN4(gbitGet(buffer[index / 4], (index % 4) * 2,     1));
      *a    = B1ToN4(gbitGet(buffer[index / 4], (index % 4) * 2 + 1, 1));
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB2:
      *r    = 
         *g = 
         *b = B2ToN4(gbitGet(buffer[index / 2], (index % 2) * 4,     2));
      *a    = B2ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 2, 2));
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB4:
      *r    = 
         *g = 
         *b = B4ToN4(gbitGet(buffer[index], 0, 4));
      *a    = B4ToN4(gbitGet(buffer[index], 4, 4));
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1:
      *r    = 
         *g = 
         *b = N1ToN4(buffer[index * 2]);
      *a    = N1ToN4(buffer[index * 2 + 1]);
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 2]);
      *r    = 
         *g = 
         *b = N2ToN4(n2[0]);
      *a    = N2ToN4(n2[1]);
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN4:
      n4 = (Gn4 *) &(buffer[index * 4]);
      *r    = 
         *g = 
         *b = n4[0];
      *a    = n4[1];
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 4]);
      *r    = 
         *g = 
         *b = RToN4(r4[0]);
      *a    = RToN4(r4[1]);
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 8]);
      *r    = 
         *g = 
         *b = RToN4(r8[0]);
      *a    = RToN4(r8[1]);
      break;



   case gimgioTypeRGB | gimgioTypeB1:
      *r = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4,     1));
      *g = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 1, 1));
      *b = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 2, 1));
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeB2:
      *r = B2ToN4(gbitGet(buffer[index], 0, 2));
      *g = B2ToN4(gbitGet(buffer[index], 2, 2));
      *b = B2ToN4(gbitGet(buffer[index], 4, 2));
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeB4:
      *r = B4ToN4(gbitGet(buffer[index * 2],     0, 4));
      *g = B4ToN4(gbitGet(buffer[index * 2],     4, 4));
      *b = B4ToN4(gbitGet(buffer[index * 2 + 1], 0, 4));
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeN1:
      *r = N1ToN4(buffer[index * 3]);
      *g = N1ToN4(buffer[index * 3 + 1]);
      *b = N1ToN4(buffer[index * 3 + 2]);
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 6]);
      *r = N2ToN4(n2[0]);
      *g = N2ToN4(n2[1]);
      *b = N2ToN4(n2[2]);
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeN4:  
      n4 = (Gn4 *) &(buffer[index * 12]);
      *r = n4[0];
      *g = n4[1];
      *b = n4[2];
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 12]);
      *r = RToN4(r4[0]);
      *g = RToN4(r4[1]);
      *b = RToN4(r4[2]);
      *a = Gn4MAX;
      break;

   case gimgioTypeRGB | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 24]);
      *r = RToN4(r8[0]);
      *g = RToN4(r8[1]);
      *b = RToN4(r8[2]);
      *a = Gn4MAX;
      break;



   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB1:
      *r = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4,     1));
      *g = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 1, 1));
      *b = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 2, 4));
      *a = B1ToN4(gbitGet(buffer[index / 2], (index % 2) * 4 + 3, 4));;
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB2:
      *r = B2ToN4(gbitGet(buffer[index], 0, 2));
      *g = B2ToN4(gbitGet(buffer[index], 2, 2));
      *b = B2ToN4(gbitGet(buffer[index], 4, 2));
      *a = B2ToN4(gbitGet(buffer[index], 6, 2));
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB4:
      *r = B4ToN4(gbitGet(buffer[index * 2],     0, 4));
      *g = B4ToN4(gbitGet(buffer[index * 2],     4, 4));
      *b = B4ToN4(gbitGet(buffer[index * 2 + 1], 0, 4));
      *a = B4ToN4(gbitGet(buffer[index * 2 + 1], 4, 4));
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1:
      *r = N1ToN4(buffer[index * 4]);
      *g = N1ToN4(buffer[index * 4 + 1]);
      *b = N1ToN4(buffer[index * 4 + 2]);
      *a = N1ToN4(buffer[index * 4 + 3]);
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 8]);
      *r = N2ToN4(n2[0]);
      *g = N2ToN4(n2[1]);
      *b = N2ToN4(n2[2]);
      *a = N2ToN4(n2[3]);
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN4:
      n4 = (Gn4 *) &(buffer[index * 16]);
      *r = n4[0];
      *g = n4[1];
      *b = n4[2];
      *a = n4[3];
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 16]);
      *r = RToN4(r4[0]);
      *g = RToN4(r4[1]);
      *b = RToN4(r4[2]);
      *a = RToN4(r4[3]);
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 32]);
      *r = RToN4(r8[0]);
      *g = RToN4(r8[1]);
      *b = RToN4(r8[2]);
      *a = RToN4(r8[3]);
      break;
   }
}

/******************************************************************************
func: gimgioGetPixelAtR

Get the pixel value in reals.
******************************************************************************/
gimgioAPI void gimgioGetPixelAtR(GimgioType const type, Gi4 const index, void * const pixel,
   Gr * const r, Gr * const g, Gr * const b, Gr * const a)
{
   // This should be replace with Real version of the above for better accuracy.
   Gn4 _r,
       _g,
       _b,
       _a;

   gimgioGetPixelAtN(type, index, pixel, &_r, &_g, &_b, &_a);
   *r = N4ToR(_r);
   *g = N4ToR(_g);
   *b = N4ToR(_b);
   *a = N4ToR(_a);
}

/******************************************************************************
func: gimgioGetRow

Get the current row.
******************************************************************************/
gimgioAPI Gi4 gimgioGetRow(Gimgio const * const img)
{
   genter;

   greturnIf(!img, 0);

   greturn img->row;
}

/******************************************************************************
func: gimgioGetTypeFile

Get the file pixel format type.
******************************************************************************/
gimgioAPI GimgioType gimgioGetTypeFile(Gimgio const * const img)
{
   genter;
   
   greturnIf(!img, gimgioTypeNONE);

   greturn img->format;
}

/******************************************************************************
func: gimgioGetTypePixel

Get the input pixel format type.
******************************************************************************/
gimgioAPI GimgioType gimgioGetTypePixel(Gimgio const * const img)
{
   genter;
   
   greturnIf(!img, gimgioTypeNONE);

   greturn img->typePixel;
}

/******************************************************************************
func: gimgioGetWidth

Get the width of the current image.
******************************************************************************/
gimgioAPI Gi4 gimgioGetWidth(Gimgio const * const img)
{
   genter;

   greturnIf(!img, 0);

   greturn img->width;
}

/******************************************************************************
func: gimgioIsStarted

Determine if the routine have started.
******************************************************************************/
gimgioAPI Gb gimgioIsStarted(void)
{
   return gbTRUE;
}

/******************************************************************************
func: gimgioOpen_

Open an image file for reading, writing or appending.
******************************************************************************/
gimgioAPI Gimgio *gimgioOpen_(Gs const * const fileName, GimgioOpenMode const mode, 
   GimgioFormat const format)
{
   genter;

   Gimgio *img;
#if defined(GIMGIO_TIFF)
   Char   *ctemp;
#endif

   greturnNullIf(
      !fileName                ||
      mode   == gimgioOpenNONE ||
      format == gimgioFormatNONE);

   img = gmemCreateType(Gimgio);
   greturnNullIf(!img);

   loopOnce
   {
      img->mode     = mode;
      img->format   = format;
      img->fileName = gsCreateFrom(fileName);

      if (mode == gimgioOpenREAD)
      {
#if defined(GIMGIO_TIFF)
         if      (format == gimgioFormatTIFF)
         {
            ctemp = gsCreateA(fileName);
            img->tiffFile = TIFFOpen((const char *) ctemp, "r");
            gmemDestroy(ctemp);
         }
         else 
#endif
         if       (format == gimgioFormatGIF)
         {
            // No open of the file.  That is left to the library.
         }
         else
         {
            img->file = gfileOpen(fileName, gfileOpenModeREAD_ONLY);
         }
      }
      else 
      {
#if defined(GIMGIO_TIFF)
         if      (format == gimgioFormatTIFF)
         {
            ctemp = gsCreateA(fileName);
            img->tiffFile = TIFFOpen((const char *) ctemp, "w");
            gmemDestroy(ctemp);
         }
         else 
#endif
         if (format == gimgioFormatGIF)
         {
            // No open of the file.  That is left to the library.
         }
         else
         {
            img->file = gfileOpen(fileName, gfileOpenModeREAD_WRITE);
         }
      }

#if defined(GIMGIO_TIFF)
      if      (format == gimgioFormatTIFF) 
      {
         breakIf(!img->tiffFile);
      }
      else
#endif
      if (format == gimgioFormatGIF)
      {
         // nothing to do.
      }
      else
      {
         breakIf(!img->file);
      }

      // Read in the basic information.
      breakIf(!_Start(img));

      if (img->mode == gimgioOpenREAD)
      {
         // Read in the basic information of the file.
         breakIf(!img->ReadStart(img));
      }

      greturn img;
   }

   // Clean up
#if defined(GIMGIO_TIFF)
   if (img->tiffFile) 
   {
      TIFFClose(img->tiffFile);
   }
#endif
   gfileClose(img->file);
   gmemDestroy(img);

   greturn NULL;
}

/******************************************************************************
func: gimgioSetCompression

Set the compression amount of the file.  
percent ranges from 0 - 100.  100 means full compression.
******************************************************************************/
gimgioAPI Gb gimgioSetCompression(Gimgio * const img, Gr const percent)
{
   genter;

   greturnFalseIf(
      !img || 
      img->mode == gimgioOpenREAD);

   // 0-1 internally
   img->compression = (Gr4) (percent / 100.);

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioSetHeight

Set the height of the image.  Set the page first before setting the
height in a multi page image format.
******************************************************************************/
gimgioAPI Gb gimgioSetHeight(Gimgio * const img, Gi4 const height)
{
   genter;

   // reading is dictated by the file.
   greturnFalseIf(
      !img || 
      img->mode == gimgioOpenREAD);

   img->height = height;

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioSetImageIndex

Set the image we want to read from the file.
******************************************************************************/
gimgioAPI Gb gimgioSetImageIndex(Gimgio * const img, Gi4 const index)
{
   genter;

   greturnFalseIf(
      index >= img->imageCount ||
      !img->SetImageIndex(img, index));

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioSetPixelRow

Set the row to work on.
******************************************************************************/
gimgioAPI Gb gimgioSetPixelRow(Gimgio * const img, void * const pixel)
{
   Gb result;
   
   genter;

   greturnFalseIf(
      !img                        ||
      img->mode == gimgioOpenREAD ||
      img->row  >= img->height);

   result = img->SetPixelRow(img, pixel);

   greturn result;
}

/******************************************************************************
func: gimgioSetPixelAtN

Set the pixel in the pixel buffer.
******************************************************************************/
gimgioAPI Gb gimgioSetPixelAtN(GimgioType const type, Gi4 const index, void * const pixel,
   Gn4 const r, Gn4 const g, Gn4 const b, Gn4 const a)
{
   Gn1 *buffer;
   Gn2 *n2;
   Gn4 *n4;
   Gr4 *r4;
   Gr8 *r8;

   buffer = (Gn1 *) pixel;

   switch(type)
   {
   case gimgioTypeBLACK | gimgioTypeB1:
      gbitSet(buffer[index / 8], index % 8, 1, N4ToB1(r));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeB2:
      gbitSet(buffer[index / 4], (index % 4) * 2, 2, N4ToB2(r));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeB4:
      gbitSet(buffer[index / 2], (index % 2) * 4, 4, N4ToB4(r));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeN1:
      buffer[index] = N4ToN1(r);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeN2:
      n2  = (Gn2 *) &(buffer[index * 2]);
      *n2 = N4ToN2(r);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeN4:
      n4  = (Gn4 *) &(buffer[index * 4]);
      *n4 = (Gn4) r;
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeR4:
      r4  = (Gr4 *) &(buffer[index * 4]);
      *r4 = (Gr4) N4ToR(r);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeR8:
      r8  = (Gr8 *) &(buffer[index * 8]);
      *r8 = N4ToR(r);
      return gbTRUE;



   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB1:
      gbitSet(buffer[index / 4], (index % 4) * 2,     1, N4ToB1(r));
      gbitSet(buffer[index / 4], (index % 4) * 2 + 1, 1, N4ToB1(a));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB2:
      gbitSet(buffer[index / 2], (index % 2) * 4,     2, N4ToB2(r));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 2, 2, N4ToB2(a));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB4:
      gbitSet(buffer[index], 0, 4, N4ToB4(r));
      gbitSet(buffer[index], 4, 4, N4ToB4(a));
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1:
      buffer[index * 2]     = N4ToN1(r);
      buffer[index * 2 + 1] = N4ToN1(a);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 2]);
      n2[0] = N4ToN2(r);
      n2[1] = N4ToN2(a);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN4:
      n4 = (Gn4 *) &(buffer[index * 4]);
      n4[0] = (Gn4) r;
      n4[1] = (Gn4) a;
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 4]);
      r4[0] = (Gr4) N4ToR(r);
      r4[1] = (Gr4) N4ToR(a);
      return gbTRUE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 8]);
      r8[0] = N4ToR(r);
      r8[1] = N4ToR(a);
      return gbTRUE;



   case gimgioTypeRGB | gimgioTypeB1:
      gbitSet(buffer[index / 2], (index % 2) * 4,     1, N4ToB1(r));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 1, 1, N4ToB1(g));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 2, 4, N4ToB1(b));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeB2:
      gbitSet(buffer[index], 0, 2, N4ToB2(r));
      gbitSet(buffer[index], 2, 2, N4ToB2(g));
      gbitSet(buffer[index], 4, 2, N4ToB2(b));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeB4:
      gbitSet(buffer[index * 2],     0, 4, N4ToB4(r));
      gbitSet(buffer[index * 2],     4, 4, N4ToB4(g));
      gbitSet(buffer[index * 2 + 1], 0, 4, N4ToB4(b));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeN1:
      buffer[index * 3]     = N4ToN1(r);
      buffer[index * 3 + 1] = N4ToN1(g);
      buffer[index * 3 + 2] = N4ToN1(b);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 6]);
      n2[0] = N4ToN2(r);
      n2[1] = N4ToN2(g);
      n2[2] = N4ToN2(b);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeN4:  
      n4 = (Gn4 *) &(buffer[index * 12]);
      n4[0] = (Gn4) r;
      n4[1] = (Gn4) g;
      n4[2] = (Gn4) b;
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 12]);
      r4[0] = (Gr4) N4ToR(r);
      r4[1] = (Gr4) N4ToR(g);
      r4[2] = (Gr4) N4ToR(b);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 24]);
      r8[0] = N4ToR(r);
      r8[1] = N4ToR(g);
      r8[2] = N4ToR(b);
      return gbTRUE;



   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB1:
      gbitSet(buffer[index / 2], (index % 2) * 4,     1, N4ToB1(r));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 1, 1, N4ToB1(g));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 2, 4, N4ToB1(b));
      gbitSet(buffer[index / 2], (index % 2) * 4 + 3, 4, N4ToB1(a));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB2:
      gbitSet(buffer[index], 0, 2, N4ToB2(r));
      gbitSet(buffer[index], 2, 2, N4ToB2(g));
      gbitSet(buffer[index], 4, 2, N4ToB2(b));
      gbitSet(buffer[index], 6, 2, N4ToB2(a));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB4:
      gbitSet(buffer[index * 2],     0, 4, N4ToB4(r));
      gbitSet(buffer[index * 2],     4, 4, N4ToB4(g));
      gbitSet(buffer[index * 2 + 1], 0, 4, N4ToB4(b));
      gbitSet(buffer[index * 2 + 1], 4, 4, N4ToB4(a));
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1:
      buffer[index * 4]     = N4ToN1(r);
      buffer[index * 4 + 1] = N4ToN1(g);
      buffer[index * 4 + 2] = N4ToN1(b);
      buffer[index * 4 + 3] = N4ToN1(a);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2:
      n2 = (Gn2 *) &(buffer[index * 8]);
      n2[0] = N4ToN2(r);
      n2[1] = N4ToN2(g);
      n2[2] = N4ToN2(b);
      n2[3] = N4ToN2(a);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN4:
      n4 = (Gn4 *) &(buffer[index * 16]);
      n4[0] = (Gn4) r;
      n4[1] = (Gn4) g;
      n4[2] = (Gn4) b;
      n4[3] = (Gn4) a;
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR4:
      r4 = (Gr4 *) &(buffer[index * 16]);
      r4[0] = (Gr4) N4ToR(r);
      r4[1] = (Gr4) N4ToR(g);
      r4[2] = (Gr4) N4ToR(b);
      r4[3] = (Gr4) N4ToR(a);
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR8:
      r8 = (Gr8 *) &(buffer[index * 32]);
      r8[0] = N4ToR(r);
      r8[1] = N4ToR(g);
      r8[2] = N4ToR(b);
      r8[3] = N4ToR(a);
      return gbTRUE;
   }

   return gbFALSE;
}

/******************************************************************************
func: gimgioSetPixelAtR

Set the pixel with real values.
******************************************************************************/
gimgioAPI Gb gimgioSetPixelAtR(GimgioType const type, Gi4 const index, void * const pixel,
   Gr const r, Gr const g, Gr const b, Gr const a)
{
   // To be replaced with a real version of the above.
   return gimgioSetPixelAtN(type, index, pixel, RToN4(r), RToN4(g), RToN4(b), RToN4(a));
}

/******************************************************************************
func: gimgioSetRow

Set the row to read or write.
******************************************************************************/
gimgioAPI Gb gimgioSetRow(Gimgio * const img, Gi4 const index)
{
   genter;

   greturnFalseIf(!img);
   
   img->row = index;

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioSetTypeFile

Set the file pixel type.  May return FALSE if you provided a bad
format.  A new format that is as close as to the selected format
is set in it's place.
******************************************************************************/
gimgioAPI Gb gimgioSetTypeFile(Gimgio * const img, GimgioType const type)
{
   Gb result;
   
   genter;

   greturnFalseIf(
      !img || 
      img->mode == gimgioOpenREAD);

   img->typeFile = type;

   // The format may have other ideas on what is supportable.
   result = img->SetTypeFile(img);

   greturn result;
}

/******************************************************************************
func: gimgioSetTypePixel

Set the input/output pixel buffer format
******************************************************************************/
gimgioAPI Gb gimgioSetTypePixel(Gimgio * const img, GimgioType const type)
{
   genter;

   greturnFalseIf(!img);

   img->typePixel = type;

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioSetWidth

Set the image width.
******************************************************************************/
gimgioAPI Gb gimgioSetWidth(Gimgio * const img, Gi4 const width)
{
   genter;

   greturnFalseIf(
      !img ||
      img->mode == gimgioOpenREAD);

   img->width = width;

   greturn gbTRUE;
}

/******************************************************************************
func: gimgioStart

Start the gimgio routines.
******************************************************************************/
gimgioAPI Gb gimgioStart(void)
{
   genter;
   greturn gbTRUE;
}

/******************************************************************************
func: gimgioStop

Clean up.
******************************************************************************/
gimgioAPI void gimgioStop(void)
{
   genter;
   greturn;
}

/******************************************************************************
local: 
function:
******************************************************************************/
/******************************************************************************
func: _Start

Set up the callbacks.
******************************************************************************/
static Gb _Start(Gimgio * const img)
{
   genter;

   // try to figure the format from the extension.  Override the 
   // provided format if set.
   switch(img->format)
   {
   case gimgioFormatBMP:  greturnFalseIf(!bmpioCreateContent( img)); break;
   case gimgioFormatGIF:  greturn gbFALSE; //greturnFalseIf(!gifioCreateContent( img)); break;
   case gimgioFormatGRAW: greturnFalseIf(!grawioCreateContent(img)); break;
#if defined(GIMGIO_JPG)
   case gimgioFormatJPG:  greturnFalseIf(!jpgioCreateContent( img)); break;
#endif
#if defined(GIMGIO_PNG)
   case gimgioFormatPNG:  greturnFalseIf(!pngioCreateContent( img)); break;
#endif
   case gimgioFormatPPM:  greturn gbFALSE; //greturnFalseIf(!ppmioCreateContent(img)); break;
   case gimgioFormatRLE:  greturn gbFALSE; //greturnFalseIf(!rleioCreateContent(img)); break;
   case gimgioFormatTRG:  greturn gbFALSE; //greturnFalseIf(!trgioCreateContent(img));  break;
#if defined(GIMGIO_ITFF)
   case gimgioFormatTIFF: greturnFalseIf(!tifioCreateContent( img)); break;
#endif
   }

   greturn gbTRUE;
}
