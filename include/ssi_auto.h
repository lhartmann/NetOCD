// SSI tabular format helper
// Make sure all expressions are valid at the time of inclusion of this file.

#ifndef SSI_AUTO_H
#define SSI_AUTO_H

///////////////////////////////////////////////////////////////
// Stage 1: Create an enum for referencing the SSI variables //
///////////////////////////////////////////////////////////////
#define SSI_TAGVAL(SSINAME, FORMAT, CNAME) \
	SSI_TAG_##SSINAME,
#define SSI_TAGFCN(SSINAME, FCN) \
	SSI_TAG_##SSINAME,

enum {
#include "ssi_auto_table.h"
};

#undef SSI_TAGVAL
#undef SSI_TAGFCN

///////////////////////////////////////////////////////////////
// Stage 2: Create the name-lookup-table for the SSI library //
///////////////////////////////////////////////////////////////
#define SSI_TAGVAL(SSINAME, FORMAT, CNAME) \
	[SSI_TAG_##SSINAME] = #SSINAME,
#define SSI_TAGFCN(SSINAME, FCN) \
	[SSI_TAG_##SSINAME] = #SSINAME,

static const char *g_pcConfigSSITags[] = {
#include "ssi_auto_table.h"
};

#undef SSI_TAGVAL
#undef SSI_TAGFCN

//////////////////////////////////////////
// Stage 3: Create the handler function //
//////////////////////////////////////////
#define SSI_TAGVAL(SSINAME, FORMAT, CNAME) \
	case SSI_TAG_##SSINAME: \
		snprintf(pcInsert, iInsertLen, FORMAT, CNAME); \
		break;
#define SSI_TAGFCN(SSINAME, FCN) \
	case SSI_TAG_##SSINAME: \
		FCN(pcInsert, iInsertLen); \
		break;

static int32_t ssi_handler(int32_t iIndex, char *pcInsert, int32_t iInsertLen)
{
	switch (iIndex) {
#include "ssi_auto_table.h"
	default:
		snprintf(pcInsert, iInsertLen, "[[Invalid SSI id=%d]]", iIndex);
		break;
	}

	/* Tell the server how many characters to insert */
	return (strlen(pcInsert));
}

#undef SSI_TAGVAL
#undef SSI_TAGFCN

///////////////////////////////////
// Stage 4: Create the init code //
///////////////////////////////////
static inline void ssi_auto_init() {
	http_set_ssi_handler((tSSIHandler) ssi_handler, g_pcConfigSSITags,
		sizeof (g_pcConfigSSITags) / sizeof (g_pcConfigSSITags[0]));
}

#endif // SSI_AUTO_H
