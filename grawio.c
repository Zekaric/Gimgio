/******************************************************************************

file:       rawio.c
author:     Robbert de Groot
copyright:  2008-2008, Robbert de Groot

description:
png file handling.

******************************************************************************/

/******************************************************************************
include: 
******************************************************************************/
#include "precompiled.h"

/******************************************************************************
local: 
type:
******************************************************************************/
// GRAW + [channels] + [channel size] + W + [9 chars for width] + H + [9 chars for height]
#define headerSIZE (4 + 4 + 4 + 1 + 9 + 1 + 9)

typedef struct 
{
   GfileIndex currentPos;
   Gb         isFileSet;
   Gn1       *row;
} Grawio;

/******************************************************************************
prototype:
******************************************************************************/
// callbacks.
static void _GrawDestroyContent(  Gimgio * const img);

static Gb   _GrawGetPixelRow(     Gimgio * const img, void * const pixel);

static Gb   _GrawReadStart(       Gimgio * const img);

static Gb   _GrawSetImageIndex(   Gimgio * const img, Gi4 const index);
static Gb   _GrawSetPixelRow(     Gimgio * const img, void * const pixel);
static Gb   _GrawSetTypeFile(     Gimgio * const img);

// local only.
static Gb   _StartFile(           Gimgio * const img, Grawio * const data);

/******************************************************************************
global: to library only
function: 
******************************************************************************/
/******************************************************************************
func: grawioCreateContent

Create the content for handling the png file.
******************************************************************************/
Gb grawioCreateContent(Gimgio * const img)
{
   Grawio *data;

   genter;

   data = gmemCreateType(Grawio);
   greturnFalseIf(!data);

   img->data           = data;
   img->DestroyContent = _GrawDestroyContent;
   img->GetPixelRow    = _GrawGetPixelRow;
   img->ReadStart      = _GrawReadStart;
   img->SetImageIndex  = _GrawSetImageIndex;
   img->SetPixelRow    = _GrawSetPixelRow;
   img->SetTypeFile    = _GrawSetTypeFile;

   greturn gbTRUE;
}

/******************************************************************************
local: 
function:
******************************************************************************/
/******************************************************************************
func: _GrawDestroyContent

Clean up.
******************************************************************************/
static void _GrawDestroyContent(Gimgio * const img)
{
   Grawio *data;

   genter;

   data = (Grawio *) img->data;

   // Other clean up common to both.
   gmemDestroy(data->row);
   gmemDestroy(data);
   img->data = NULL;

   greturn;
}

/******************************************************************************
func: _GrawGetPixelRow

Read in a pixel row.
******************************************************************************/
static Gb _GrawGetPixelRow(Gimgio * const img, void * const pixel)
{
   Grawio *data;
   Gi8     position;
   
   genter;

   data = (Grawio *) img->data;

   // Set the position in the file.
   position = 
      data->currentPos + 
      headerSIZE       + 
      img->row * gimgioGetPixelSize(img->typeFile, img->width);
   greturnFalseIf(!gfileSetPosition(img->file, gpositionSTART, position));

   // Allocate the row.
   if (!data->row)
   {
      data->row = gmemCreateTypeArray(Gn1, gimgioGetPixelSize(img->typeFile, img->width));
      greturnFalseIf(!data->row);
   }

   // Get the pixels for the row.
   gfileGet(
      img->file, 
      gimgioGetPixelSize(img->typeFile, img->width),
      data->row);

   // Convert the pixel row to what we want.
   gimgioConvert(
      img->width,
      img->typeFile,
      data->row,
      img->typePixel,
      pixel);

   greturn gbTRUE;
}

/******************************************************************************
func: _GrawReadStart

Read in the image information.
******************************************************************************/
static Gb _GrawReadStart(Gimgio * const img)
{
   // no genter and greturn because of setjmp.
   Char    ctemp[10];
   Grawio *data;

   genter;

   data = (Grawio *) img->data;

   data->currentPos = gfileGetPosition(img->file);

   // Check to see if the file is a GRAW.
   gmemClear(ctemp, 10);
   gfileGet(img->file, 4, ctemp);
   greturnFalseIf(strcmp(ctemp, "GRAW"));

   // Get the raw information.
   gmemClear(ctemp, 10);
   gfileGet(img->file, 4, ctemp);
   greturnFalseIf(strcmp(ctemp, "RGB "));

   gmemClear(ctemp, 10);
   gfileGet(img->file, 4, ctemp);
   greturnFalseIf(strcmp(ctemp, "N1  "));
   img->typeFile = gimgioTypeRGB | gimgioTypeN1;
   
   gmemClear(ctemp, 10);
   gfileGet(img->file, 1, ctemp);
   greturnFalseIf(strcmp(ctemp, "W"));

   gmemClear(ctemp, 10);
   gfileGet(img->file, 9, ctemp);
   img->width = atoi(ctemp);
   
   gmemClear(ctemp, 10);
   gfileGet(img->file, 1, ctemp);
   greturnFalseIf(strcmp(ctemp, "H"));

   gmemClear(ctemp, 10);
   gfileGet(img->file, 9, ctemp);
   img->height = atoi(ctemp);

   greturn gbTRUE;
}

/******************************************************************************
func: _GrawSetImageIndex

GRAW doesn't support multiple images yet.
******************************************************************************/
static Gb _GrawSetImageIndex(Gimgio * const img, Gi4 const index)
{
   genter;
   img; index;
   return gbFALSE;
}

/******************************************************************************
func: _GrawSetPixelRow

Set the pixels of a row.
******************************************************************************/
static Gb _GrawSetPixelRow(Gimgio * const img, void * const pixel)
{
   Grawio *data;
   Gi8     position;

   genter;

   data = (Grawio *) img->data;

   if (!data->isFileSet)
   {
      greturnFalseIf(!_StartFile(img, data));
   }

   gimgioConvert(
      img->width, 
      img->typePixel,
      pixel,
      img->typeFile,
      data->row);

   position = 
      data->currentPos +
      headerSIZE       +
      img->row * gimgioGetPixelSize(img->typeFile, img->width);
   greturnFalseIf(!gfileSetPosition(img->file, gpositionSTART, position));

   gfileSet(
      img->file, 
      gimgioGetPixelSize(img->typeFile, img->width),
      data->row,
      NULL);

   greturn gbTRUE;
}

/******************************************************************************
func: _GrawSetTypeFile

Ensure the file type is proper.
******************************************************************************/
static Gb _GrawSetTypeFile(Gimgio * const img)
{
   Gb result;

   genter;

   result = (img->typeFile == (gimgioTypeRGB | gimgioTypeN1)) ? gbTRUE : gbFALSE;
   
   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   greturn result;
}

/******************************************************************************
func: _StartFile

Write out the header and write out blank rows for the entire image.
******************************************************************************/
static Gb _StartFile(Gimgio * const img, Grawio * const data)
{
   Gi4 row;
   Gn1 header[headerSIZE + 1];

   genter;

   sprintf_s((Char *) header, headerSIZE + 1, "GRAWRGB N1  W% 9dH% 9d", img->width, img->height);
   
   // Ensure the file type is what we expect.
   img->typeFile = gimgioTypeRGB | gimgioTypeN1;

   // Get our current position in case it may be inside another file.
   data->currentPos = gfileGetPosition(img->file);
   gfileSet(img->file, headerSIZE, header, NULL);

   // Create the row buffer.
   data->row = gmemCreateTypeArray(Gn1, gimgioGetPixelSize(img->typeFile, img->width));
   greturnFalseIf(!data->row);

   gmemClear(data->row, gimgioGetPixelSize(img->typeFile, img->width));
   forCount(row, img->height)
   {
      gfileSet(img->file, gimgioGetPixelSize(img->typeFile, img->width), data->row, NULL);
   }

   data->isFileSet = gbTRUE;

   greturn gbTRUE;
}
