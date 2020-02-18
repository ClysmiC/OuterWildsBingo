#include "bingo_gen.h"

#include <cstdarg>
#include <stdio.h>

// Global state

char *					g_pChzMan;			// Manifest
int						g_iMan;				// Index
int						g_nLine;			// Line #

DynamicArray<Tag>						g_aryTag;
DynamicArray<Synergy>					g_arySynrg;
DynamicPoolAllocator<TagExpression>		g_dpaTagexpr;

// Implementation

void ErrorAndExit(const char * errFormat, ...)
{
	if (g_nLine >= 0)
	{
		fprintf(stderr, "[Error / Line %d]: ", g_nLine);
	}

	va_list arglist;
	va_start(arglist, errFormat);
	vfprintf(stderr, errFormat, arglist);
	va_end(arglist);
	fflush(stderr);

	Assert(false);

	exit(EXIT_FAILURE);
}

StringView StrvNextCell()
{
	// NOTE (andrews) This function does not advance to the next line. Make sure to call SkipToNextLine when appropriate.

	StringView strvResult;
	strvResult.m_cCh = 0;

	bool fQuoted = (g_pChzMan[g_iMan] ==  '"');
	if (fQuoted)
	{
		strvResult.m_pCh = g_pChzMan + g_iMan + 1;
	}
	else
	{
		strvResult.m_pCh = g_pChzMan + g_iMan;
	}

	if (fQuoted)
	{
		if (!tryConsumeUntilChar(g_pChzMan, &g_iMan, '"'))		ErrorAndExit("Expected '\"'");

		strvResult.m_cCh = g_pChzMan + g_iMan - strvResult.m_pCh;

		if (!tryConsumeChar(g_pChzMan, &g_iMan, '"'))			ErrorAndExit("Expected '\"'");
		if (!tryConsumeChar(g_pChzMan, &g_iMan, ','))			ErrorAndExit("Expected ','");
	}
	else
	{
		if (!tryConsumeUntilChar(g_pChzMan, &g_iMan, ','))		ErrorAndExit("Expected ','");

		strvResult.m_cCh = g_pChzMan + g_iMan - strvResult.m_pCh;

		if (!tryConsumeChar(g_pChzMan, &g_iMan, ','))			ErrorAndExit("Expected ','");
	}

	return strvResult;
}

void SkipToNextLine()
{
	if (!tryConsumeUntilChar(g_pChzMan, &g_iMan, '\n'))			ErrorAndExit("Expected new line");
	if (!tryConsumeChar(g_pChzMan, &g_iMan, '\n'))				ErrorAndExit("Expected new line");

	g_nLine++;
}

TAGID TagidFromStrv(const StringView & strv)
{
	for (int iTag = 0; iTag < g_aryTag.cItem; iTag++)
	{
		if (strv == g_aryTag[iTag].m_strv)		return g_aryTag[iTag].m_tagid;
	}

	// NOTE (andrews) Any unknown tag lookup ultimately results in an error. To free the caller of the burden of error
	//	checking, we report it right here.

	char * pChzStrvCopy = new char[strv.m_cCh + 1];
	memcpy(pChzStrvCopy, strv.m_pCh, strv.m_cCh);
	pChzStrvCopy[strv.m_cCh] = '\0';

	ErrorAndExit("Unknown tag: %s", pChzStrvCopy);
	return TAGID_Nil;
}

void CompileTagExpression(const char * pCh, int cCh, TagExpression * poTagexpr)
{
	// TODO
}

int main()
{
	// Init globals

	init(&g_aryTag);
	init(&g_arySynrg);
	init(&g_dpaTagexpr);

	// Read in manifest

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];
	memset((void *)(g_pChzMan), 0, cChManifestMax + 1);
	g_iMan = 0;
	g_nLine = -1;

	{
		// TODO: Allow command line override

		char * pchzManifestFile = "./res/manifest.csv";


		FILE * file = fopen(pchzManifestFile, "rb");
		Defer(if (file) fclose(file));

		int cB = file ? fread(g_pChzMan, 0x1, cChManifestMax + 1, file) : -1;
		if (cB < 0)
		{
			ErrorAndExit("Error opening manifest file");
			return 1;
		}
		else if (cB == cChManifestMax + 1)
		{
			fprintf(stderr, "Manifest fi of size >= %d bytes are not yet supported", cBManifestMax);
			return 1;
		}
	}

	g_nLine = 1;		// NOTE (andrew) Spreadsheet has 1-based line count

	// Tags

	{
		// Verify header

		{
			StringView strv = StrvNextCell();
			if (strv != "!!Tags")		ErrorAndExit("Expected \"!!Tags\"");

			SkipToNextLine();	// Skip !!Tags
			SkipToNextLine();	// Skip Headers
		}

		// Parse tag list

		int cTag = 0;
		while (true)
		{
			// Tag

			Tag * pTag = nullptr;

			{
				StringView strv = StrvNextCell();
				if (strv.m_cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				pTag = appendNew(&g_aryTag);
				pTag->m_strv = strv;
				pTag->m_tagid = TAGID(cTag);
				cTag++;
			}

			// Implies

			{
				StringView strv = StrvNextCell();
				if (strv.m_cCh == 0)
				{
					pTag->m_pTagexprImplied = nullptr;
				}
				else
				{
					pTag->m_pTagexprImplied = allocate(&g_dpaTagexpr);
					CompileTagExpression(strv.m_pCh, strv.m_cCh, pTag->m_pTagexprImplied);
				}
			}

			// Max per row

			{
				static const char * s_pChzCol = "Max Per Row";

				StringView strv = StrvNextCell();
				if (strv.m_cCh == 0)												ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				int iChStrv = 0;
				if (!tryConsumeInt(strv.m_pCh, &iChStrv, &pTag->m_cMaxPerRow))		ErrorAndExit("Expected integer in \"%s\"", s_pChzCol);
				if (pTag->m_cMaxPerRow <= 0)										ErrorAndExit("\"%s\" must be > 0", s_pChzCol);
			}

			// Self synergy

			{
				static const char * s_pChzCol = "Self Synergy";

				StringView strv = StrvNextCell();
				if (strv.m_cCh == 0)												ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				float gSynergy;
				int iChStrv = 0;
				if (!tryConsumeFloatApprox(strv.m_pCh, &iChStrv, &gSynergy))		ErrorAndExit("Expected float in \"%s\"", s_pChzCol);

				if (!FloatEq(gSynergy, 0, 0.01))
				{
					Synergy * pSynrg = appendNew(&g_arySynrg);
					pSynrg->m_tagid0 = pTag->m_tagid;
					pSynrg->m_tagid1 = pTag->m_tagid;
					pSynrg->m_nSynergy = gSynergy;
				}
			}

			SkipToNextLine();
		}
	}

	// Synergies

	{
		// Verify header

		{
			StringView strv = StrvNextCell();
			if (strv != "!!Synergies")		ErrorAndExit("Expected \"!!Synergies\"");

			SkipToNextLine();	// Skip !!Synergies
			SkipToNextLine();	// Skip Headers
		}

		// Parse synergy list

		//while (true)
		//{
		//	
		//	SkipToNextLine();
		//}
	}
}