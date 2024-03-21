/******************************************************************************

file:       pngio.c
author:     Robbert de Groot
copyright:  2008-2008, Robbert de Groot

description:
png file handling.

******************************************************************************/

/******************************************************************************
include: 
******************************************************************************/
#include "precompiled.h"
#include "tp_png/spng.h"

/******************************************************************************
local: 
type:
******************************************************************************/
typedef struct 
{
   spng_ctx          *pngContext;
   struct spng_ihdr   pngHeader;
   int                pngFormat;
   Gcount             pngFileByteCount;
   Gn1               *pngFileByteList;
   size_t             pngImageSize;
   Gn1               *pngImage;
} Pngio;

/******************************************************************************
prototype:
******************************************************************************/
// callbacks.
static void _PngDestroyContent(  Gimgio * const img);

static Gb   _PngGetPixelRow(     Gimgio * const img, void *const pixel);

static Gb   _PngReadStart(       Gimgio * const img);

static Gb   _PngSetImageIndex(   Gimgio * const img, Gi4 const index);
static Gb   _PngSetPixelRow(     Gimgio * const img, void * const pixel);
static Gb   _PngSetTypeFile(     Gimgio * const img);

// completely local
#if 0
static Gb   _CreateRowPointers(  Gimgio * const img, Pngio * const data, Gcount const rowSize);

static void _DestroyRowPointers( Gimgio * const img, Pngio * const data);
#endif

static Gb   _WritePng(           Gimgio * const img, Pngio * const data);

/******************************************************************************
global: to library only
function: 
******************************************************************************/
/******************************************************************************
func: pngioCreateContent

Create the content for handling the png file.
******************************************************************************/
Gb pngioCreateContent(Gimgio * const img)
{
   Pngio *data;

   genter;

   data = gmemCreateType(Pngio);
   greturnFalseIf(!data);

   img->data           = data;
   img->DestroyContent = _PngDestroyContent;
   img->GetPixelRow    = _PngGetPixelRow;
   img->ReadStart      = _PngReadStart;
   img->SetImageIndex  = _PngSetImageIndex;
   img->SetPixelRow    = _PngSetPixelRow;
   img->SetTypeFile    = _PngSetTypeFile;

   greturn gbTRUE;
}

/******************************************************************************
local: 
function:
******************************************************************************/
#if 0
/******************************************************************************
func: _CreateRowPointers

Create the row pointers.
******************************************************************************/
static Gb _CreateRowPointers(Gimgio * const img, Pngio * const data, Gcount const rowSize)
{
   genter;

   Gindex a;

   // Allocate the 'row pointers', image buffer.
   data->row = memCreateTypeArray(Gn1 *, img->height);
   returnFalseIf(!data->row);

   // Allocate the rows.
   forCount(a, img->height) 
   {
      data->row[a] = memCreateTypeArray(Gn1, rowSize);
      returnFalseIf(!data->row[a]);
   }

   greturn gbTRUE;
}

/******************************************************************************
func: _DestroyRowPointers

Destroy the row pointers.
******************************************************************************/
static void _DestroyRowPointers(Gimgio * const img, Pngio * const data)
{
   genter;

   Gi4 a;

   if (data->row)
   {
      forCount(a, img->height)
      {
         memDestroy(data->row[a]);
      }

      memDestroy(data->row);
   }
   data->row = NULL;

   greturn;
}
#endif

/******************************************************************************
func: _PngDestroyContent

Clean up.
******************************************************************************/
static void _PngDestroyContent(Gimgio * const img)
{
   genter;

   Pngio *data;

   data = (Pngio *) img->data;

   // Reading
   if (img->mode == gimgioOpenREAD)
   {
      memDestroy(data->pngImage);
   }
   // Writing
   else 
   {
      _WritePng(img, data);
   }

   //_DestroyRowPointers(img, data);

   gmemDestroy(img->data);
   img->data = NULL;

   greturn;
}

/******************************************************************************
func: _PngGetPixelRow

Read in a pixel row.
******************************************************************************/
static Gb _PngGetPixelRow(Gimgio * const img, void * const pixel)
{
   genter;

   Pngio *data;

   data = (Pngio *) img->data;

   // Convert the pixel row to what we want.
   gimgioConvert(
      img->width,
      img->typeFile,
      &data->pngImage[4 * img->width * img->row],
      img->typePixel,
      pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _PngReadStart

Read in the image information.
******************************************************************************/
static Gb _PngReadStart(Gimgio * const img)
{
   genter;

   Gb                 result;
   Pngio             *data;
   size_t             limit = ((size_t) 1024 * 1024) * 64;
   int                ret;

   result = gbFALSE;
   data   = (Pngio *) img->data;

   data->pngContext = spng_ctx_new(0);

   breakScope
   {
      // Read in the file
      breakIf(!gfileGetContent(img->file, &data->pngFileByteCount, &data->pngFileByteList));

      spng_set_crc_action(  data->pngContext, SPNG_CRC_USE, SPNG_CRC_USE);
      spng_set_chunk_limits(data->pngContext, limit, limit);
      spng_set_png_buffer(  data->pngContext, data->pngFileByteList, data->pngFileByteCount);

      ret = spng_get_ihdr(data->pngContext, &data->pngHeader);
      breakIf(ret);

      img->width        = (Gcount) data->pngHeader.width;
      img->height       = (Gcount) data->pngHeader.height;
      
      data->pngFormat = SPNG_FMT_PNG;
      if      (data->pngHeader.color_type == SPNG_COLOR_TYPE_GRAYSCALE)
      {
         img->typeFile = gimgioTypeBLACK;
      }
      else if (data->pngHeader.color_type == SPNG_COLOR_TYPE_TRUECOLOR)
      {
         img->typeFile = gimgioTypeRGB;
      }
      else if (data->pngHeader.color_type == SPNG_COLOR_TYPE_INDEXED)
      {
         img->typeFile   = gimgioTypeRGB;
         data->pngFormat = SPNG_FMT_RGBA8;
      }
      else if (data->pngHeader.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA)
      {
         img->typeFile = gimgioTypeBLACK | gimgioTypeALPHA;
      }
      else if (data->pngHeader.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA)
      {
         img->typeFile = gimgioTypeRGB | gimgioTypeALPHA;
      }
      else
      {
         break;
      }

      if      (data->pngHeader.bit_depth == 8)
      {
         img->typeFile |= gimgioTypeN1;
      }
      else if (data->pngHeader.bit_depth == 16)
      {
         img->typeFile |= gimgioTypeN2;
      }
      else
      {
         break;
      }

#if 0
      struct spng_plte plte = {0};
      ret = spng_get_plte(ctx, &plte);

      if (ret && 
          ret != SPNG_ECHUNKAVAIL)
      {
         printf("spng_get_plte() error: %s\n", spng_strerror(ret));
         goto error;
      }

      if (!ret) 
      {
         printf("palette entries: %u\n", plte.n_entries);
      }
#endif

      img->format       = gimgioFormatPNG;
      img->imageCount   = 1;
      img->imageIndex   = 0;
      img->row          = 0;

      spng_decoded_image_size(data->pngContext, data->pngFormat, &data->pngImageSize);

      data->pngImage = memCreateTypeArray(Gn1, data->pngImageSize);
      if (!data->pngImage)
      {
         memDestroy(data->pngFileByteList);
         break;
      }

      ret = spng_decode_image(
         data->pngContext, 
         data->pngImage, 
         data->pngImageSize, 
         data->pngFormat, 
         0);

      result = gbTRUE;
   }

   // Clean up
   memDestroy(data->pngFileByteList);
   data->pngFileByteList = NULL;

   spng_ctx_free(data->pngContext);
   data->pngContext = NULL;

   greturn gbTRUE;
}

/******************************************************************************
func: _PngSetImageIndex

Png doesn't support multiple images per file.
******************************************************************************/
static Gb _PngSetImageIndex(Gimgio * const img, Gi4 const index)
{
   genter;

   img; index;
   
   greturn gbFALSE;
}

/******************************************************************************
func: _PngSetPixelRow

Set the pixels of a row.
******************************************************************************/
static Gb _PngSetPixelRow(Gimgio * const img, void * const pixel)
{
   genter;

   Pngio *data;

   data = (Pngio *) img->data;

   gimgioConvert(
      img->width, 
      img->typePixel,
      pixel,
      img->typeFile,
      &data->pngImage[4 * img->width * img->row]);

   greturn gbTRUE;
}

/******************************************************************************
func: _PngSetTypeFile

Ensure the given type is allowed.  If not default to something valid.
******************************************************************************/
static Gb _PngSetTypeFile(Gimgio * const img)
{
   genter;

   switch(img->typeFile)
   {
   case gimgioTypeBLACK | gimgioTypeB1:
   case gimgioTypeBLACK | gimgioTypeB2:
   case gimgioTypeBLACK | gimgioTypeB4:
   case gimgioTypeBLACK | gimgioTypeN1:
   case gimgioTypeBLACK | gimgioTypeN2:
      // supported by PNG.
      return gbFALSE; 
   
   case gimgioTypeBLACK | gimgioTypeN4:
   case gimgioTypeBLACK | gimgioTypeR4:
   case gimgioTypeBLACK | gimgioTypeR8:
      img->typeFile = gimgioTypeBLACK | gimgioTypeN2;
      return gbFALSE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB1:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB2:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeB4:
      img->typeFile = gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1;
      return gbFALSE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2:
      // supported by PNG.
      return gbFALSE;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN4:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR4:
   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeR8:
      img->typeFile = gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2;
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeB1:
   case gimgioTypeRGB | gimgioTypeB2:
   case gimgioTypeRGB | gimgioTypeB4:
      img->typeFile = gimgioTypeRGB | gimgioTypeN1;
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeN1:
   case gimgioTypeRGB | gimgioTypeN2:
      // supported by PNG.
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeN4:
   case gimgioTypeRGB | gimgioTypeR4:
   case gimgioTypeRGB | gimgioTypeR8:
      img->typeFile = gimgioTypeRGB | gimgioTypeN2;
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB1:
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB2:
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeB4:
      img->typeFile = gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1;
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1:
      // supported by PNG.
      return gbTRUE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2:
      return gbFALSE;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN4:
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR4:
   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeR8:
      img->typeFile = gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2;
      return gbFALSE;
   }

   // Should never reach this.
   img->typeFile = gimgioTypeRGB | gimgioTypeN1;
   
   greturn gbFALSE;
}

/******************************************************************************
func: _WritePng

Write out the png file.
******************************************************************************/
static Gb _WritePng(Gimgio * const img, Pngio * const data) 
{
   genter;

   int    error;
   size_t size;

   // Specify image dimensions, PNG format 
   struct spng_ihdr ihdr =
   {
       .width      = img->width,
       .height     = img->height,
       .bit_depth  = 8,
       .color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA
   };

   // Creating an encoder context requires a flag
   data->pngContext = spng_ctx_new(SPNG_CTX_ENCODER);

   // Encode to internal buffer managed by the library
   spng_set_option(data->pngContext, SPNG_ENCODE_TO_BUFFER, 1);

   // Image will be encoded according to ihdr.color_type, .bit_depth
   spng_set_ihdr(data->pngContext, &ihdr);

   // SPNG_FMT_PNG is a special value that matches the format in ihdr,
   // SPNG_ENCODE_FINALIZE will finalize the PNG with the end-of-file marker
   spng_encode_image(
      data->pngContext, 
      data->pngImage, 
      data->pngImageSize, 
      SPNG_FMT_PNG, 
      SPNG_ENCODE_FINALIZE);

   // PNG is written to an internal buffer by default
   error = gbFALSE;
   data->pngFileByteList = spng_get_png_buffer(data->pngContext, &size, &error);
   if (data->pngFileByteList)
   {
      data->pngFileByteCount = (Gcount) size;
      if (!gfileStoreContent(
            img->fileName, 
            data->pngFileByteCount, 
            data->pngFileByteList))
      {
         error = gbTRUE;
      }
   }

   // User owns the buffer after a successful call
   free(data->pngFileByteList);

   // Free context memory 
   spng_ctx_free(data->pngContext);

   greturn !error;

#if 0
   /* Allocate basic libpng structures */
   data->pptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
   gotoIf(!data->pptr, _WritePngERROR);
   
   data->iptr = png_create_info_struct(data->pptr);
   gotoIf(!data->iptr, _WritePngERROR);
   
   /* setjmp() must be called in every function
   ** that calls a PNG-reading libpng function */
   gotoIf(setjmp(png_jmpbuf(data->pptr)), _WritePngERROR);
   
   png_set_write_fn(data->pptr, (void *) img->file, _Pwrite, NULL);

   /* set the zlib compression level */
   png_set_compression_level(data->pptr, (int) (img->compression * Z_BEST_COMPRESSION));
   
   /* write PNG info to structure */
   switch(img->typeFile)
   {
   case gimgioTypeBLACK | gimgioTypeB1:
      // may be needed
      //png_set_packswap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         1,
         PNG_COLOR_TYPE_GRAY, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeB2:
      // may be needed
      //png_set_packswap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         2,
         PNG_COLOR_TYPE_GRAY, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeB4:
      // may be needed
      //png_set_packswap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         4,
         PNG_COLOR_TYPE_GRAY, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeN1:
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         8,
         PNG_COLOR_TYPE_GRAY, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeN2:
      png_set_swap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         16,
         PNG_COLOR_TYPE_GRAY, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN1:
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         8,
         PNG_COLOR_TYPE_GRAY_ALPHA, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeBLACK | gimgioTypeALPHA | gimgioTypeN2:
      png_set_swap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         16,
         PNG_COLOR_TYPE_GRAY_ALPHA, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeRGB | gimgioTypeN1:
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         8,
         PNG_COLOR_TYPE_RGB, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeRGB | gimgioTypeN2:
      png_set_swap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         16,
         PNG_COLOR_TYPE_RGB, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN1:
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         8,
         PNG_COLOR_TYPE_RGB_ALPHA, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;

   case gimgioTypeRGB | gimgioTypeALPHA | gimgioTypeN2:
      png_set_swap(data->pptr);
      png_set_IHDR(
         data->pptr, 
         data->iptr, 
         (png_uint_32) img->width, 
         (png_uint_32) img->height, 
         16,
         PNG_COLOR_TYPE_RGB_ALPHA, 
         PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, 
         PNG_FILTER_TYPE_DEFAULT);
      break;
   }
   
   png_write_info(data->pptr, data->iptr);
   
   /* now we can go ahead and just write the whole image */
   png_write_image(data->pptr, data->row);
   
   png_write_end(data->pptr, 0);
   
   png_destroy_write_struct(&data->pptr, &data->iptr);
   
   return gbTRUE;


_WritePngERROR:

   if (data->iptr)
   {
      png_destroy_write_struct(&data->pptr, &data->iptr);
   }
   else
   {
      png_destroy_write_struct(&data->pptr, NULL);
   }

   greturn gbFALSE;
#endif
}
