/******************************************************************************

file:       jpgio.c
author:     Robbert de Groot
copyright:  2008-2008, Robbert de Groot

description:
jpg file handling.

******************************************************************************/

/******************************************************************************
include: 
******************************************************************************/
#include "precompiled.h"

#if defined(GIMGIO_JPG)

#include "setjmp.h"
#include "jpeglib.h"
#include "jerror.h"

/******************************************************************************
local: 
type:
******************************************************************************/
#define BUF_SIZE  4096	/* choose an efficiently fwrite'able size */

#pragma warning(disable:4324)

typedef struct 
{
   struct jpeg_error_mgr       pub;             /* "public" fields */
   jmp_buf                     setjmp_buffer;   /* for greturn to caller */
} JerrorMgr;

typedef JerrorMgr * JerrorMgrPtr;

typedef struct 
{
   struct jpeg_destination_mgr pub;             /* public fields */
   Gfile                      *outfile;         /* target stream */
   JOCTET                     *buffer;          /* start of buffer */
} JdestMgr;

typedef JdestMgr * JdestMgrPtr;

typedef struct 
{
   struct jpeg_source_mgr      pub;             /* public fields */
   Gfile                      *infile;          /* source stream */
   JOCTET                     *buffer;          /* start of buffer */
   boolean                     start_of_file;   /* have we gotten any data yet? */
} JsrcMgr;

typedef JsrcMgr * JsrcMgrPtr;

typedef struct 
{
   struct jpeg_compress_struct    wcinfo;
   struct jpeg_decompress_struct  rcinfo;
   JerrorMgr                      jerr;
   JSAMPARRAY                     buffer;     /* Output row buffer */
   Gi4                            row_stride; /* physical row width in output buffer */
   Gn1                           *btemp;
   JsrcMgr                       *jsrc;
   JdestMgr                      *jdest;
   Gn1                          **row;
   JSAMPROW                      *row_pointer;	
} Jpgio;

/******************************************************************************
prototype:
******************************************************************************/
// callbacks.
static void _JpgDestroyContent(  Gimgio * const img);

static Gb   _JpgGetPixelRow(     Gimgio * const img, void *const pixel);

static Gb   _JpgReadStart(       Gimgio * const img);

static Gb   _JpgSetImageIndex(   Gimgio * const img, Gi4 const index);
static Gb   _JpgSetPixelRow(     Gimgio * const img, void * const pixel);
static Gb   _JpgSetTypeFile(     Gimgio * const img);

// completely local
static Gb   _CreateRowPointers(  Gimgio * const img, Jpgio * const data, Gi4 const rowSize);

static void _DestroyRowPointers( Gimgio * const img, Jpgio * const data);

static Gb   _ReadJpg(            Gimgio * const img, Jpgio * const data);

static Gb   _WriteJpg(           Gimgio * const img, Jpgio * const data);

// jpeg stuff.
static void    _JdestStart(         j_compress_ptr cinfo);
static void    _JdestStop(          j_compress_ptr cinfo);
static boolean _JdestSet(           j_compress_ptr cinfo);

static void    _JerrorHandler(      j_common_ptr cinfo);

static boolean _JsrcFillInputBuffer(j_decompress_ptr cinfo);
static void    _JsrcSkip(           j_decompress_ptr cinfo, long num_bytes);
static void    _JsrcStart(          j_decompress_ptr cinfo);
static void    _JsrcStop(           j_decompress_ptr cinfo);

/******************************************************************************
global: to library only
function: 
******************************************************************************/
/******************************************************************************
func: jpgioCreateContent

Create the content for handling the jpg file.
******************************************************************************/
Gb jpgioCreateContent(Gimgio * const img)
{
   Jpgio *data;

   genter;

   data = gmemCreateType(Jpgio);
   greturnFalseIf(!data);

   img->data           = data;
   img->DestroyContent = _JpgDestroyContent;
   img->GetPixelRow    = _JpgGetPixelRow;
   img->ReadStart      = _JpgReadStart;
   img->SetImageIndex  = _JpgSetImageIndex;
   img->SetPixelRow    = _JpgSetPixelRow;
   img->SetTypeFile    = _JpgSetTypeFile;

   greturn gbTRUE;
}

/******************************************************************************
local: 
function:
******************************************************************************/
/******************************************************************************
func: _CreateRowPointers

Create the row pointers.
******************************************************************************/
static Gb _CreateRowPointers(Gimgio * const img, Jpgio * const data, Gi4 const rowSize)
{
   Gi4 a;

   // Allocate the 'row pointers', image buffer.
   data->row = memCreateTypeArray(Gn1 *, img->height);
   returnFalseIf(!data->row);

   // Allocate the rows.
   forCount(a, img->height) 
   {
      data->row[a] = memCreateTypeArray(Gn1, rowSize);
      returnFalseIf(!data->row[a]);
   }

   return gbTRUE;
}

/******************************************************************************
func: _DestroyRowPointers

Destroy the row pointers.
******************************************************************************/
static void _DestroyRowPointers(Gimgio * const img, Jpgio * const data)
{
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
}

/******************************************************************************
func: _JpgDestroyContent

Clean up.
******************************************************************************/
static void _JpgDestroyContent(Gimgio * const img)
{
   Jpgio *data;

   data = (Jpgio *) img->data;

   // Reading
   if (img->mode == gimgioOpenREAD)
   {
      /* Establish the setjmp greturn context for my_error_exit to use. */
      gotoIf(setjmp(data->jerr.setjmp_buffer), _JpgDestroyContentERROR);


      /* Step 7: Finish decompression */

      jpeg_finish_decompress(&data->rcinfo);
      /* We can ignore the greturn value since suspension is not possible
      ** with the stdio data source. */

      /* Step 8: Release JPEG decompression object */

      /* This is an important step since it will release a good deal of memory. */
      jpeg_destroy_decompress(&data->rcinfo);

      /* At this point you may want to check to see whether any corrupt-data
      ** warnings occurred (test whether jerr.pub.num_warnings is nonzero). */
   }
   // Writing
   else 
   {
      _WriteJpg(img, data);
   }

_JpgDestroyContentERROR:

   _DestroyRowPointers(img, data);
   gmemDestroy(img->data);
   img->data = NULL;
}

/******************************************************************************
func: _ReadJpg

Read in the image if not read already.
******************************************************************************/
static Gb _ReadJpg(Gimgio * const img, Jpgio * const data)
{
   // For PNG error handling. 
   gotoIf(setjmp(data->jerr.setjmp_buffer), _ReadJpgERROR);


   /* Failed to create image */
   gotoIf(!_CreateRowPointers(img, data, data->row_stride), _ReadJpgERROR);

   /* Here we use the library's state variable cinfo.output_scanline as the
   ** loop counter, so that we don't have to keep track ourselves. */
   while (data->rcinfo.output_scanline < data->rcinfo.output_height) 
   {
      /* jpeg_read_scanlines expects an array of pointers to scanlines.
      ** Here the array is only one element long, but you could ask for
      ** more than one scanline at a time if that's more convenient. */
      jpeg_read_scanlines(&data->rcinfo, data->buffer, 1);

      /* Assume put_scanline_someplace wants a pointer and sample count. */
      gmemCopyOverAt(
         data->row[data->rcinfo.output_scanline - 1], 
         data->row_stride,
         0,
         *data->buffer,
         0);
   }

   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   return gbTRUE;

_ReadJpgERROR:
   /* Clean up. */
   _DestroyRowPointers(img, data);

   return gbFALSE;
}

/******************************************************************************
func: _JpgGetPixelRow

Read in a pixel row.
******************************************************************************/
static Gb _JpgGetPixelRow(Gimgio * const img, void * const pixel)
{
   Jpgio *data;

   data = (Jpgio *) img->data;

   // Read in the png file.
   if (!data->row)
   {
      returnFalseIf(!_ReadJpg(img, data));
   }

   // Convert the pixel row to what we want.
   gimgioConvert(
      img->width,
      img->typeFile,
      data->row[img->row],
      img->typePixel,
      pixel);

   return gbTRUE;
}

/******************************************************************************
func: _JpgReadStart

Read in the image information.
******************************************************************************/
static Gb _JpgReadStart(Gimgio * const img)
{
   // no genter and greturn because of setjmp.

   Jpgio *data;

   data = (Jpgio *) img->data;

   /* Step 1: allocate and initialize JPEG decompression object */

   /* We set up the normal JPEG error routines, then override error_exit. */
   data->rcinfo.err = jpeg_std_error(&data->jerr.pub);
   data->jerr.pub.error_exit = _JerrorHandler;

   /* Establish the setjmp greturn context for my_error_exit to use. */
   gotoIf(setjmp(data->jerr.setjmp_buffer), _JpgReadStartERROR);

   /* Now we can initialize the JPEG decompression object. */
   jpeg_create_decompress(&data->rcinfo);

   /* Step 2: specify data source (eg, a file) */
   data->rcinfo.src = (struct jpeg_source_mgr *) 
      (*data->rcinfo.mem->alloc_small)(
         (j_common_ptr) &data->rcinfo, 
         JPOOL_PERMANENT,
         sizeof(JsrcMgr));

   data->jsrc = (JsrcMgrPtr) data->rcinfo.src;
   
   data->jsrc->buffer = (JOCTET *) 
      (*data->rcinfo.mem->alloc_small)(
         (j_common_ptr) &data->rcinfo, 
         JPOOL_PERMANENT,
         BUF_SIZE * sizeof(JOCTET));

   data->jsrc->pub.init_source       = _JsrcStart;
   data->jsrc->pub.fill_input_buffer = _JsrcFillInputBuffer;
   data->jsrc->pub.skip_input_data   = _JsrcSkip;
   data->jsrc->pub.resync_to_restart = jpeg_resync_to_restart;
   data->jsrc->pub.term_source       = _JsrcStop;
   data->jsrc->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
   data->jsrc->pub.next_input_byte   = NULL; /* until buffer loaded */
   data->jsrc->infile                = img->file;

   /* Step 3: read file parameters with jpeg_read_header() */
   jpeg_read_header(&data->rcinfo, TRUE);

   /* Step 4: set parameters for decompression */
   data->rcinfo.dct_method           = JDCT_ISLOW;
   data->rcinfo.out_color_components = 3;
   data->rcinfo.out_color_space      = JCS_RGB;

   /* Step 5: Start decompressor */
   jpeg_start_decompress(&data->rcinfo);

   /* We can ignore the greturn value since suspension is not possible
   ** with the stdio data source. */

   /* We may need to do some setup of our own at this point before reading
   ** the data.  After jpeg_start_decompress() we have the correct scaled
   ** output image dimensions available, as well as the output colormap
   ** if we asked for color quantization.
   ** In this example, we need to make an output work buffer of the right size. */ 
   /* JSAMPLEs per row in output buffer */
   img->width  = data->rcinfo.output_width;
   img->height = data->rcinfo.output_height;

   data->row_stride = (int) (img->width * data->rcinfo.output_components);

   /* Make a one-row-high sample array that will go away when done with image */
   data->buffer = (*data->rcinfo.mem->alloc_sarray)
      ((j_common_ptr) &data->rcinfo, JPOOL_IMAGE, data->row_stride, 1);

   return gbTRUE;

_JpgReadStartERROR:
   /* If we get here, the JPEG code has signaled an error.
   ** We need to clean up the JPEG object, close the input file, and greturn. */
   jpeg_destroy_decompress(&data->rcinfo);

   return gbFALSE;
}

/******************************************************************************
func: _JpgSetImageIndex

Jpeg doesn't support multiple images per file.
******************************************************************************/
static Gb _JpgSetImageIndex(Gimgio * const img, Gi4 const index)
{
   img; index;
   return gbFALSE;
}

/******************************************************************************
func: _JpgSetPixelRow

Set the pixels of a row.
******************************************************************************/
static Gb _JpgSetPixelRow(Gimgio * const img, void * const pixel)
{
   Jpgio *data;

   data = (Jpgio *) img->data;

   if (!data->row)
   {
      returnFalseIf(
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

   return gbTRUE;
}

/******************************************************************************
func: _JpgSetTypeFile

Make sure the file image type is valid.  Point to something valid.
******************************************************************************/
static Gb _JpgSetTypeFile(Gimgio * const img)
{
   Gb result;

   result = (img->typeFile == (gimgioTypeRGB | gimgioTypeN1)) ? gbTRUE : gbFALSE;

   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   return result;
}

/******************************************************************************
func: _WriteJpg

Write out the png file.
******************************************************************************/
static Gb _WriteJpg(Gimgio * const img, Jpgio * const data) 
{
   /* Step 1: allocate and initialize JPEG compression object */
   
   /* We have to set up the error handler first, in case the initialization
   ** step fails.  (Unlikely, but it could happen if you are out of memory.)
   ** This routine fills in the contents of struct jerr, and greturns jerr's
   ** address which we place into the link field in cinfo. */
   data->wcinfo.err = jpeg_std_error(&data->jerr.pub);

   data->jerr.pub.error_exit = _JerrorHandler;

   /* Establish the setjmp greturn context for my_error_exit to use. */
   gotoIf(setjmp(data->jerr.setjmp_buffer), _WriteJpgERROR);

   /* Now we can initialize the JPEG compression object. */
   jpeg_create_compress(&data->wcinfo);

   /* Step 2: specify data destination (eg, a file) */
   /* Note: steps 2 and 3 can be done in either order. */
   data->wcinfo.dest = (struct jpeg_destination_mgr *) 
      (*data->wcinfo.mem->alloc_small)(
         (j_common_ptr) &data->wcinfo, 
         JPOOL_PERMANENT,
         sizeof(JdestMgr));

   data->jdest = (JdestMgrPtr) data->wcinfo.dest;

   data->jdest->pub.init_destination    = _JdestStart;
   data->jdest->pub.empty_output_buffer = _JdestSet;
   data->jdest->pub.term_destination    = _JdestStop;
   data->jdest->outfile                 = img->file;

   /* Step 3: set parameters for compression */

   /* First we supply a description of the input image.
   ** Four fields of the cinfo struct must be filled in: */
   data->wcinfo.dct_method       = JDCT_ISLOW;
   data->wcinfo.image_width      = (JDIMENSION) img->width; 	/* image width and height, in pixels */
   data->wcinfo.image_height     = (JDIMENSION) img->height;
   data->wcinfo.input_components = 3;		/* # of color components per pixel */
   data->wcinfo.in_color_space   = JCS_RGB; 	/* colorspace of input image */

   /* Now use the library's routine to set default compression parameters.
   ** (You must set at least cinfo.in_color_space before calling this,
   ** since the defaults depend on the source color space.) */
   jpeg_set_defaults(&data->wcinfo);

   /* Now you can set any non-default parameters you wish to.
   ** Here we just illustrate the use of quality (quantization table) scaling: */
   jpeg_set_quality(
      &data->wcinfo, 
      (int) (255. * (-img->compression + 1.)),
      TRUE);

   /* Step 4: Start compressor */

   /* TRUE ensures that we will write a complete interchange-JPEG file.
   ** Pass TRUE unless you are very sure of what you're doing. */
   jpeg_start_compress(&data->wcinfo, TRUE);

   /* Step 5: while (scan lines remain to be written) */
   /*           jpeg_write_scanlines(...); */

   /* Here we use the library's state variable cinfo.next_scanline as the
   ** loop counter, so that we don't have to keep track ourselves.
   ** To keep things simple, we pass one scanline per call; you can pass
   ** more if you wish, though. */
   data->row_stride = (int) (img->width * 3); /* JSAMPLEs per row in image_buffer */

   while (data->wcinfo.next_scanline < data->wcinfo.image_height) 
   {
      /* jpeg_write_scanlines expects an array of pointers to scanlines.
      ** Here the array is only one element long, but you could pass
      ** more than one scanline at a time if that's more convenient. */
      data->row_pointer = (JSAMPROW *) &data->row[data->wcinfo.next_scanline];
      jpeg_write_scanlines(&data->wcinfo, data->row_pointer, 1);
   }

   /* Step 6: Finish compression */

   jpeg_finish_compress(&data->wcinfo);

   /* Step 7: release JPEG compression object */

   /* This is an important step since it will release a good deal of memory. */
   jpeg_destroy_compress(&data->wcinfo);

   return gbTRUE;


_WriteJpgERROR:

   /* If we get here, the JPEG code has signaled an error.
   ** We need to clean up the JPEG object, close the input file, and greturn. */
   jpeg_destroy_compress(&data->wcinfo);

   return gbFALSE;
}


/******************************************************************************
func: _JdestStart, _JdestStop, _JdestSet

File IO for compression.
******************************************************************************/
void _JdestStart(j_compress_ptr cinfo) 
{
   JdestMgrPtr dest = (JdestMgrPtr) cinfo->dest;

   /* Allocate the output buffer --- it will be released when done with image */
   dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)(
      (j_common_ptr) cinfo, 
      JPOOL_IMAGE,
      BUF_SIZE * sizeof(JOCTET));

   dest->pub.next_output_byte = dest->buffer;
   dest->pub.free_in_buffer   = BUF_SIZE;
}

void _JdestStop(j_compress_ptr cinfo) 
{
   JdestMgrPtr dest      = (JdestMgrPtr) cinfo->dest;
   int         datacount = (int) (BUF_SIZE - dest->pub.free_in_buffer);

   /* Write any data remaining in the buffer */
   if (datacount > 0) 
   {
      if (!gfileSet((Gfile *) dest->outfile, datacount, dest->buffer, NULL)) 
      {
         ERREXIT(cinfo, JERR_FILE_WRITE);
      }
   }
}

boolean _JdestSet(j_compress_ptr cinfo) 
{
   JdestMgrPtr dest = (JdestMgrPtr) cinfo->dest;

   if (!gfileSet((Gfile *) dest->outfile, BUF_SIZE, dest->buffer, NULL)) 
   {
      ERREXIT(cinfo, JERR_FILE_WRITE);
   }

   dest->pub.next_output_byte = dest->buffer;
   dest->pub.free_in_buffer   = BUF_SIZE;

   return TRUE;
}

/******************************************************************************
func: _JerrorHandler 

Handles the critical errors.
******************************************************************************/
void _JerrorHandler(j_common_ptr cinfo) 
{
   /* cinfo->err really points to a JerrorMgr struct, so coerce pointer */
   JerrorMgrPtr myerr = (JerrorMgrPtr) cinfo->err;

   /* Always display the message. */
   /* We could postpone this until after greturning, if we chose. */
   (*cinfo->err->output_message) (cinfo);

   /* Return control to the setjmp point */
   longjmp(myerr->setjmp_buffer, 1);
}

/******************************************************************************
func: _JsrcStart _JsrcFillInputBuffer _JsrcSkip _JsrcStop

Decompression file access functions.
******************************************************************************/
void _JsrcStart(j_decompress_ptr cinfo) 
{
   JsrcMgrPtr src = (JsrcMgrPtr) cinfo->src;

   /* We reset the empty-input-file flag for each image,
   ** but we don't clear the input buffer.
   ** This is correct behavior for reading a series of images from one source. */
   src->start_of_file = TRUE;
}

boolean _JsrcFillInputBuffer(j_decompress_ptr cinfo) 
{
   JsrcMgrPtr src = (JsrcMgrPtr) cinfo->src;
   size_t     nbytes;

   nbytes = (size_t) gfileGet((Gfile *) src->infile, BUF_SIZE, src->buffer);

   if (nbytes <= 0) 
   {
      /* Treat empty input file as fatal error */
      if (src->start_of_file) 
      {
         ERREXIT(cinfo, JERR_INPUT_EMPTY);
      }

      WARNMS(cinfo, JWRN_JPEG_EOF);

      /* Insert a fake EOI marker */
      src->buffer[0] = (JOCTET) 0xFF;
      src->buffer[1] = (JOCTET) JPEG_EOI;
      nbytes         = 2;
   }

   src->pub.next_input_byte = src->buffer;
   src->pub.bytes_in_buffer = nbytes;
   src->start_of_file       = FALSE;

   return TRUE;
}

void _JsrcSkip(j_decompress_ptr cinfo, long num_bytes) 
{
   JsrcMgrPtr src = (JsrcMgrPtr) cinfo->src;

   /* Just a dumb implementation for now.  Could use fseek() except
   ** it doesn't work on pipes.  Not clear that being smart is worth
   ** any trouble anyway --- large skips are infrequent. */
   if (num_bytes > 0) 
   {
      while (num_bytes > (long) src->pub.bytes_in_buffer) 
      {
         num_bytes -= (long) src->pub.bytes_in_buffer;
         
         _JsrcFillInputBuffer(cinfo);
         /* note we assume that fill_input_buffer will never greturn FALSE,
         ** so suspension need not be handled. */
      }

      src->pub.next_input_byte += (size_t) num_bytes;
      src->pub.bytes_in_buffer -= (size_t) num_bytes;
   }
}

void _JsrcStop(j_decompress_ptr cinfo) 
{
   /* no work necessary here */
   cinfo;
}

#endif
