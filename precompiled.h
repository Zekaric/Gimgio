/******************************************************************************
file:       Precompiled
author:     Robbert de Groot
company:    Zekaric
copyright:  2008, Zekaric

description:
precompiled header for Gimgio
******************************************************************************/

// Uncomment the formats that are compiled into the library.
//#define GIMGIO_TIFF 1
#define GIMGIO_PNG  1
//#define GIMGIO_JPG  1

/******************************************************************************
include:
******************************************************************************/
#include "grl.h"
#include "gimgio.h"

// These are built in and do not require an external library.
#include "bmpio.h"
#include "grawio.h"

#if defined(GIMGIO_JPG)
#include "jpgio.h"
#endif

#if defined(GIMGIO_PNG)
#include "pngio.h"
#endif

#if defined(GIMGIO_TIFF)
#include "tifio.h"
#endif
