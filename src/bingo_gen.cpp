// View this file with tab-spacing 4. On GitHub, append ?ts=4 to the URL to do so.

#include "bingo_gen.h"

#include "tag_expr.h"

#include <cstdarg>
#include <stdio.h>

// Global state

char *		g_pChzMan;					// Manifest
int			g_iChMan;					// Index
int			g_nLine;					// Line #
bool		g_fEof;						// Have we hit the end of current file
bool		g_fIsGeneratedMan;			// Are we processing the generated manifest?

DynamicArray<Tag>						g_aryTag;
DynamicArray<Synergy>					g_arySynrg;
DynamicArray<Shorthand>					g_aryShorthand;
DynamicPoolAllocator<TagExpression>		g_dpaTagexpr;



// Implementation

void ErrorAndExit(const char * errFormat, ...)
{
	if (g_nLine >= 0)
	{
		fprintf(stderr, "[Error on line %d%s]: ", g_nLine, g_fIsGeneratedMan ? " (generated)" : "");
	}

	va_list arglist;
	va_start(arglist, errFormat);
	vfprintf(stderr, errFormat, arglist);
	va_end(arglist);
	fflush(stderr);

	// Assert(false);

	getchar();
	exit(EXIT_FAILURE);
}

StringView StrvNextCell()
{
	// NOTE (andrew) This function is not intended to advance to the next line. Make sure to call SkipToNextLine when appropriate.

	StringView strvResult;
	strvResult.cCh = 0;
	strvResult.pCh = g_pChzMan + g_iChMan;

	if (g_fEof)		return strvResult;

	char chMatched;
	if (!tryConsumeUntilChar(g_pChzMan, &g_iChMan, &chMatched, '\t', '\n'))		ErrorAndExit("Expected '\\t' or '\\n'");

	strvResult.cCh = g_pChzMan + g_iChMan - strvResult.pCh;

	// NOTE (andrew) Don't consume thru \n so that SkipToNextLine can handle it

	if (chMatched == '\t' && !tryConsumeChar(g_pChzMan, &g_iChMan, '\t'))						ErrorAndExit("Expected '\t'");

	trim(&strvResult);
	return strvResult;
}

void SkipToNextLine()
{
	if (g_fEof)		return;

	static const bool s_fResultOnMatchNull = true;
	Verify(tryConsumeUntilChar(g_pChzMan, &g_iChMan, '\n', s_fResultOnMatchNull));

	// NOTE (andrew) Line bump in either case to make a virtual empty line at end of file

	g_nLine++;

	if (g_pChzMan[g_iChMan] == '\n')
	{
		g_iChMan++;
	}
	else
	{
		// Keep g_iMan pointing at eof

		g_fEof = true;
	}
}

bool FIsLegalTagCharacter(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

TAGID TagidFromStrv(const StringView & strv)
{
	for (int iTag = 0; iTag < g_aryTag.cItem; iTag++)
	{
		if (strv == g_aryTag[iTag].m_strv)		return g_aryTag[iTag].m_tagid;
	}

	// NOTE (andrew) Any unknown tag lookup ultimately results in an error. To free the caller of the burden of error
	//	checking, we report it right here.

	char * pChzStrvCopy = new char[strv.cCh + 1];
	memcpy(pChzStrvCopy, strv.pCh, strv.cCh);
	pChzStrvCopy[strv.cCh] = '\0';

	ErrorAndExit("Unknown tag: %s", pChzStrvCopy);
	return TAGID_Nil;
}

bool FTryCompileDollarExpr(const StringView & strvCell, DynamicArray<String> * poAryStrCompiled)
{
	// E.g., "Visit $2/3/4$" planet(s) compiles into:
	//	"Visit 2 planet(s)"
	//	"Visit 3 planet(s)"
	//	"Visit 4 planet(s)"

	int iChDol0 = -1;
	int iChDol1 = -1;
	int cDol = 0;

	for (int iChCell = 0; iChCell < strvCell.cCh; )
	{
		char c = strvCell.pCh[iChCell++];

		if (c == '$')
		{
			if (cDol == 0)
			{
				iChDol0 = iChCell - 1;
			}
			else if (cDol == 1)
			{
				iChDol1 = iChCell - 1;
			}

			cDol++;
		}
	}

	if (cDol == 0)		return false;
	if (cDol != 2)		ErrorAndExit("Could not parse $-expression");

	StringView strvSub;
	strvSub.pCh = strvCell.pCh + iChDol0 + 1;
	strvSub.cCh = 0;

	for (int iCh = iChDol0 + 1; iCh < iChDol1; )
	{
		char c = strvCell.pCh[iCh++];
		strvSub.cCh++;

		if (c == '/' || iCh == iChDol1)
		{
			if (c == '/')		strvSub.cCh--;

			String * pStrCompiled = appendNew(poAryStrCompiled);

			// Init with string before opening $

			init(pStrCompiled, strvCell.pCh, iChDol0);

			// Append substituted $-expr

			append(pStrCompiled, strvSub.pCh, strvSub.cCh);

			// Reset strvSub

			strvSub.pCh = strvCell.pCh + iCh;
			strvSub.cCh = 0;
		}
	}

	int cChPostDol1 = strvCell.cCh - (iChDol1 + 1);
	for (int iStr = 0; iStr < poAryStrCompiled->cItem; iStr++)
	{
		// Append string after closing $ to finish compiling

		append(&(*poAryStrCompiled)[iStr], strvCell.pCh + iChDol1 + 1, cChPostDol1);
	}

	return true;
}

StringView StrvCompileAtExpr(const StringView & strvCell, int iDollarCtx)
{
	// E.g., "PlanetHopping & @BH;ET;DB@" compiles into the following:
	//	iDollarCtx = 0: "PlanetHopping & ((BH) | (ET) | (DB))"
	//	iDollarCtx = 1: "PlanetHopping & ((BH & ET) | (BH & DB) | (ET & DB))"
	//	iDollarCtx = 2: "PlanetHopping & ((BH & ET &DB))"

	int iChAt0 = -1;
	int iChAt1 = -1;

	{
		int cAt = 0;

		for (int iChCell = 0; iChCell < strvCell.cCh; )
		{
			char c = strvCell.pCh[iChCell++];

			if (c == '@')
			{
				if (cAt == 0)
				{
					iChAt0 = iChCell - 1;
				}
				else if (cAt == 1)
				{
					iChAt1 = iChCell - 1;
				}

				cAt++;
			}
		}

		if (cAt == 0)		return strvCell;
		if (cAt != 2)		ErrorAndExit("Could not parse @-expression");
	}

	DynamicArray<StringView> aryStrvSub;
	init(&aryStrvSub);

	{
		int iChStrvSubStart = iChAt0 + 1;
		int cChStrvSub = 0;

		for (int iCh = iChAt0 + 1; iCh < iChAt1; )
		{
			char c = strvCell.pCh[iCh++];
			cChStrvSub++;

			if (c == ';' || iCh == iChAt1)
			{
				if (c == ';')		cChStrvSub--;

				StringView * pStrvSub = appendNew(&aryStrvSub);
				pStrvSub->pCh = strvCell.pCh + iChStrvSubStart;
				pStrvSub->cCh = cChStrvSub;

				iChStrvSubStart = iCh;
				cChStrvSub = 0;
			}
		}
	}

	int n = aryStrvSub.cItem;	// As in "n choose k"
	int k = iDollarCtx + 1;		// As in "n choose k"

	Assert(k > 0);
	if (n == 0)		ErrorAndExit("Could not parse @-expression");
	if (n > 32)		ErrorAndExit("Cannot exceed 32 items in @-expression");

	typedef u32 bitfield;
	struct Combine
	{
		void operator()(int n, int k, DynamicArray<bitfield> * poAryBitfield, int nShift=0)
		{
			if (k == 0)
			{
				append(poAryBitfield, 0u);
			}
			else
			{
				for (int i = nShift; i < n - (k - 1); i++)
				{
					int cItemPrev = poAryBitfield->cItem;

					(*this)(n, k - 1, poAryBitfield, i + 1);

					for (int iBitfield = cItemPrev; iBitfield < poAryBitfield->cItem; iBitfield++)
					{
						(*poAryBitfield)[iBitfield] |= (1 << i);
					}
				}
			}
		}
	};

	DynamicArray<bitfield> aryBitfield;
	init(&aryBitfield);

	Combine()(n, k, &aryBitfield);

	// Init with string before opening @

	String strResult;
	init(&strResult, strvCell.pCh, iChAt0);

	// Append substituted @-expr
	
	append(&strResult, "(");

	for (int iBitfield = 0; iBitfield < aryBitfield.cItem; iBitfield++)
	{
		if (iBitfield > 0)		append(&strResult, " | ");

		append(&strResult, "(");
		int cBitSet = 0;
		for (int iBit = 0; iBit < n; iBit++)
		{
			if (aryBitfield[iBitfield] & (1 << iBit))
			{
				if (cBitSet > 0)		append(&strResult, " & ");

				append(&strResult, aryStrvSub[iBit].pCh, aryStrvSub[iBit].cCh);
				cBitSet++;
			}
		}

		Assert(cBitSet == k);
	
		append(&strResult, ")");
	}

	append(&strResult, ")");

	// Append string after closing @ to finish compiling

	int cChPostAt1 = strvCell.cCh - (iChAt1 + 1);
	append(&strResult, strvCell.pCh + iChAt1 + 1, cChPostAt1);

	StringView strvResult;
	strvResult.pCh = strResult.pBuffer;
	strvResult.cCh = strResult.cChar;

	return strvResult;
}

void CreateGeneratedManifestFromRaw()
{
	Assert(g_iChMan == 0);
	Assert(g_nLine == -1);

	// Read in raw manifest

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];

	{
		memset((void *)(g_pChzMan), 0, cChManifestMax + 1);

		FILE * file = fopen("./res/manifest.tsv", "rb");
		Defer(if (file) fclose(file));

		int cB = file ? fread(g_pChzMan, 0x1, cChManifestMax + 1, file) : -1;
		if (cB < 0)
		{
			ErrorAndExit("Error opening manifest file");
		}
		else if (cB == cChManifestMax + 1)
		{
			ErrorAndExit("Manifest file of size >= %d bytes is not yet supported", cBManifestMax);
		}
	}

	// Create generated manifest

	FILE * fileGenerated = fopen("./res/manifest_generated.tsv", "wb");
	Defer (if (fileGenerated) fclose(fileGenerated));

	if (!fileGenerated)
	{
		ErrorAndExit("Failed to create generated manifest file");
	}

	g_nLine = 1;
	while (true)
	{
		if (StrvNextCell() == "!!Goals")		break;

		if (g_fEof)		ErrorAndExit("Could not find !!Goals in raw manifest");
		SkipToNextLine();
	}

	SkipToNextLine();		// Skip !!Goals
	SkipToNextLine();		// Skip headers

	// Copy all text up before goals are listed

	if (fwrite(g_pChzMan, 0x1, g_iChMan, fileGenerated) != g_iChMan)
	{
		g_nLine = -1;
		ErrorAndExit("Failed to write to generated manifest file");
	}

	int iChEmptyCell = -1;

	while (true)
	{
		struct GoalGen		// tag = goalg
		{
			StringView		m_strvText;
			StringView		m_strvAlt;
			StringView		m_strvTags;
			StringView		m_strvDifficulty;
			StringView		m_strvLength;
		};

		DynamicArray<GoalGen> aryGoalg;
		init(&aryGoalg);

		// Text

		{
			StringView strv = StrvNextCell();
			if (strv.cCh == 0)
			{
				// Assume an empty first cell is an entirely empty line

				iChEmptyCell = strv.pCh - g_pChzMan;
				SkipToNextLine();
				break;
			}

			if (startsWith(strv, "[TODO]"))
			{
				SkipToNextLine();
				continue;
			}

			DynamicArray<String> aryStrSubstituted;
			init(&aryStrSubstituted);
				
			if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
			{
				for (int iStr = 0; iStr < aryStrSubstituted.cItem; iStr++)
				{
					GoalGen * pGoalg = appendNew(&aryGoalg);
					pGoalg->m_strvText.pCh = aryStrSubstituted[iStr].pBuffer;
					pGoalg->m_strvText.cCh = aryStrSubstituted[iStr].cChar;
				}
			}
			else
			{
				GoalGen * pGoalg = appendNew(&aryGoalg);
				pGoalg->m_strvText = strv;
			}
		}

		// Clarification

		{
			StringView strv = StrvNextCell();

			for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
			{
				aryGoalg[iGoal].m_strvAlt = strv;
			}
		}

		// Tags

		{
			StringView strv = StrvNextCell();

			DynamicArray<String> aryStrSubstituted;
			init(&aryStrSubstituted);
				
			if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
			{
				if (aryStrSubstituted.cItem != aryGoalg.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvTags.pCh = aryStrSubstituted[iGoal].pBuffer;
					pGoalg->m_strvTags.cCh = aryStrSubstituted[iGoal].cChar;
				}
			}
			else
			{
				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvTags = strv;
				}
			}

			for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
			{
				GoalGen * pGoalg = &aryGoalg[iGoal];
				pGoalg->m_strvTags = StrvCompileAtExpr(pGoalg->m_strvTags, iGoal);
			}
		}

		// Difficulty

		{
			static const char * s_pChzCol = "Difficulty";

			StringView strv = StrvNextCell();
			if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

			DynamicArray<String> aryStrSubstituted;
			init(&aryStrSubstituted);
				
			if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
			{
				if (aryStrSubstituted.cItem != aryGoalg.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvDifficulty.pCh = aryStrSubstituted[iGoal].pBuffer;
					pGoalg->m_strvDifficulty.cCh = aryStrSubstituted[iGoal].cChar;
				}
			}
			else
			{
				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvDifficulty = strv;
				}
			}
		}

		// Length

		{
			static const char * s_pChzCol = "Length";

			StringView strv = StrvNextCell();
			if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

			DynamicArray<String> aryStrSubstituted;
			init(&aryStrSubstituted);
				
			if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
			{
				if (aryStrSubstituted.cItem != aryGoalg.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvLength.pCh = aryStrSubstituted[iGoal].pBuffer;
					pGoalg->m_strvLength.cCh = aryStrSubstituted[iGoal].cChar;
				}
			}
			else
			{
				for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
				{
					GoalGen * pGoalg = &aryGoalg[iGoal];
					pGoalg->m_strvLength = strv;
				}
			}
		}

		for (int iGoal = 0; iGoal < aryGoalg.cItem; iGoal++)
		{
			GoalGen * pGoalg = &aryGoalg[iGoal];

			bool fFail = false;
			fFail = fFail || (fwrite(pGoalg->m_strvText.pCh, 0x1, pGoalg->m_strvText.cCh, fileGenerated) != pGoalg->m_strvText.cCh);
			fFail = fFail || (fputs("\t", fileGenerated) < 0);
			fFail = fFail || (fwrite(pGoalg->m_strvAlt.pCh, 0x1, pGoalg->m_strvAlt.cCh, fileGenerated) != pGoalg->m_strvAlt.cCh);
			fFail = fFail || (fputs("\t", fileGenerated) < 0);
			fFail = fFail || (fwrite(pGoalg->m_strvTags.pCh, 0x1, pGoalg->m_strvTags.cCh, fileGenerated) != pGoalg->m_strvTags.cCh);
			fFail = fFail || (fputs("\t", fileGenerated) < 0);
			fFail = fFail || (fwrite(pGoalg->m_strvDifficulty.pCh, 0x1, pGoalg->m_strvDifficulty.cCh, fileGenerated) != pGoalg->m_strvDifficulty.cCh);
			fFail = fFail || (fputs("\t", fileGenerated) < 0);
			fFail = fFail || (fwrite(pGoalg->m_strvLength.pCh, 0x1, pGoalg->m_strvLength.cCh, fileGenerated) != pGoalg->m_strvLength.cCh);
			fFail = fFail || (fputs("\n", fileGenerated) < 0);

			// BB (andrew) Not making any effort to prevent a ragged row here. It's fine though, since idc if it's ragged when I parse it back out

			if (fFail)
			{
				g_nLine = -1;
				ErrorAndExit("Failed to write to generated manifest file");
			}
		}

		SkipToNextLine();
	}

	// Verify eof

	{
		StringView strv = StrvNextCell();
		if (strv != "!!Eof")		ErrorAndExit("Expected \"!!Eof\"");

		SkipToNextLine();

		if (!g_fEof)				ErrorAndExit("File has content after \"!!Eof\"");
	}

	bool fFail = false;
	fFail = fFail || (fwrite(g_pChzMan + iChEmptyCell, 0x1, g_iChMan - iChEmptyCell, fileGenerated) != g_iChMan - iChEmptyCell);
	fFail = fFail || (fputs("\0", fileGenerated) < 0);

	if (fFail)
	{
		g_nLine = -1;
		ErrorAndExit("Failed to write to generated manifest file");
	}
}

int main()
{
	// Init globals for generating manifest from raw

	g_pChzMan = nullptr;
	g_iChMan = 0;
	g_nLine = -1;
	g_fEof = false;
	g_fIsGeneratedMan = false;

	CreateGeneratedManifestFromRaw();

	// Init globals for compiling generated manifest

	init(&g_aryTag);
	init(&g_arySynrg);
	init(&g_aryShorthand);
	init(&g_dpaTagexpr);
	g_pChzMan = nullptr;
	g_iChMan = 0;
	g_nLine = -1;
	g_fEof = false;
	g_fIsGeneratedMan = true;

	// Read in generated manifest

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];
	memset((void *)(g_pChzMan), 0, cChManifestMax + 1);

	{
		// TODO: Allow command line override (?)

		char * pChzManifestFile = "./res/manifest_generated.tsv";

		FILE * file = fopen(pChzManifestFile, "rb");
		Defer(if (file) fclose(file));

		int cB = file ? fread(g_pChzMan, 0x1, cChManifestMax + 1, file) : -1;
		if (cB < 0)
		{
			ErrorAndExit("Error opening manifest file");
			return 1;
		}
		else if (cB == cChManifestMax + 1)
		{
			ErrorAndExit("Manifest file of size >= %d bytes is not yet supported", cBManifestMax);
			return 1;
		}
	}

	g_nLine = 1;

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
				if (strv.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				for (int iCh = 0; iCh < strv.cCh; iCh++)
				{
					if (!FIsLegalTagCharacter(strv.pCh[iCh]))		ErrorAndExit("Tags may only contain letters and underscores");
				}

				pTag = appendNew(&g_aryTag);
				pTag->m_strv = strv;
				pTag->m_tagid = TAGID(cTag);
				cTag++;
			}

			// Implies

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)
				{
					pTag->m_pTagexprImplied = nullptr;
				}
				else
				{
					pTag->m_pTagexprImplied = PTagexprCompile(strv.pCh, strv.cCh);
				}
			}

			// Max per row

			{
				static const char * s_pChzCol = "Max Per Row";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)													ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				int iChStrv = 0;
				if (!tryConsumeInt(strv.pCh, &iChStrv, &pTag->m_cMaxPerRow))		ErrorAndExit("Expected integer in \"%s\"", s_pChzCol);
				if (pTag->m_cMaxPerRow <= 0)										ErrorAndExit("\"%s\" must be > 0", s_pChzCol);
			}

			// Self synergy

			{
				static const char * s_pChzCol = "Self Synergy";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)													ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				float gSynrg;
				int iChStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iChStrv, &gSynrg))			ErrorAndExit("Expected float in \"%s\"", s_pChzCol);

				if (!FloatEq(gSynrg, 0, 0.01))
				{
					Synergy * pSynrg = appendNew(&g_arySynrg);
					pSynrg->m_tagid0 = pTag->m_tagid;
					pSynrg->m_tagid1 = pTag->m_tagid;
					pSynrg->m_nSynrg = gSynrg;
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

		while (true)
		{
			// Tag0

			TAGID tagid0 = TAGID_Nil;

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				tagid0 = TagidFromStrv(strv);
			}

			// Tag1

			int cSynrgAdded = 0;
			{
				static const char * s_pChzCol = "Tag 1";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				StringView strvSubstr;
				strvSubstr.pCh = strv.pCh;
				strvSubstr.cCh = 0;

				int iStrv = 0;
				while (iStrv < strv.cCh)
				{
					// Split on ';'

					char c = strv.pCh[iStrv++];
					if (c != ';')
					{
						strvSubstr.cCh++;
					}

					if (iStrv == strv.cCh || strv.pCh[iStrv] == ';')
					{
						trim(&strvSubstr);

						Synergy * pSynrg = appendNew(&g_arySynrg);
						pSynrg->m_tagid0 = tagid0;
						pSynrg->m_tagid1 = TagidFromStrv(strvSubstr);
						pSynrg->m_nSynrg = 0;		// Haven't parsed this cell yet

						cSynrgAdded++;
						strvSubstr.pCh = strv.pCh + iStrv + 1;
						strvSubstr.cCh = 0;
					}
				}
			}

			// Synergy

			{
				static const char * s_pChzCol = "Synergy";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)											ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				float gSynrg = 0;
				int iStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iStrv, &gSynrg))		ErrorAndExit("Expected float in \"%s\"", s_pChzCol);

				if (!FloatEq(gSynrg, 0, 0.01))
				{
					// Patch up the synergies we just added with their correct values

					for (int iSynrgAdded = 0; iSynrgAdded < cSynrgAdded; iSynrgAdded++)
					{
						g_arySynrg[g_arySynrg.cItem - iSynrgAdded - 1].m_nSynrg = gSynrg;
					}
				}
				else
				{
					// We appendend a bunch of synergies and just now see that they have value 0... remove them

					removeLastMultiple(&g_arySynrg, cSynrgAdded);
				}
			}

			SkipToNextLine();
		}
	}

	// Shorthands

	{
		// Verify header

		{
			StringView strv = StrvNextCell();
			if (strv != "!!Shorthands")		ErrorAndExit("Expected \"!!Shorthands\"");

			SkipToNextLine();	// Skip !!Shorthands
			SkipToNextLine();	// Skip Headers
		}

		// Parse shorthand list

		while (true)
		{
			// Short

			StringView strvShort;

			{
				static const char * s_pChzCol = "Short";

				strvShort = StrvNextCell();
				if (strvShort.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				if (strvShort.cCh > 4)		ErrorAndExit("Shorthand cannot exceed 4 characters");		// NOTE (andrew) This limit is arbitrary, but anything much greater than 4 defeats the purpose
			}

			StringView strvLong;
			{
				static const char * s_pChzCol = "Long";

				strvLong = StrvNextCell();
				if (strvLong.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);
			}

			SkipToNextLine();
		}
	}

	// Goals

	{
		// Verify header

		{
			StringView strv = StrvNextCell();
			if (strv != "!!Goals")		ErrorAndExit("Expected \"!!Goals\"");

			SkipToNextLine();	// Skip !!Goals
			SkipToNextLine();	// Skip Headers
		}

		// Parse goal list

		while (true)
		{
			DynamicArray<Goal> aryGoal;
			init(&aryGoal);

			// Text

			Goal * pGoal = nullptr;

			{
				StringView strv = StrvNextCell();

				if (strv.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				pGoal = appendNew(&aryGoal);
				pGoal->m_strvText = strv;
			}

			Assert(pGoal);

			// Clarification

			{
				StringView strv = StrvNextCell();
				pGoal->m_strvAlt = strv;
			}

			// Tags

			{
				StringView strv = StrvNextCell();

				if (strv.cCh == 0)
				{
					pGoal->m_pTagexpr = nullptr;
				}
				else
				{
					pGoal->m_pTagexpr = PTagexprCompile(strv.pCh, strv.cCh);
				}
			}

			// Difficulty

			{
				static const char * s_pChzCol = "Difficulty";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				int iChStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iChStrv, &pGoal->m_gDifficulty))		ErrorAndExit("Expected float in \"%s\"", s_pChzCol);
				if (pGoal->m_gDifficulty <= 0)												ErrorAndExit("\"%s\" must be > 0", s_pChzCol);
			}

			// Length

			{
				static const char * s_pChzCol = "Length";

				StringView strv = StrvNextCell();
				if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

				int iChStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iChStrv, &pGoal->m_gLength))			ErrorAndExit("Expected float in \"%s\"", s_pChzCol);
				if (pGoal->m_gDifficulty <= 0)												ErrorAndExit("\"%s\" must be > 0", s_pChzCol);
			}

			SkipToNextLine();
		}
	}

	// Verify eof

	{
		StringView strv = StrvNextCell();
		if (strv != "!!Eof")		ErrorAndExit("Expected \"!!Eof\"");

		SkipToNextLine();

		if (!g_fEof)				ErrorAndExit("File has content after \"!!Eof\"");
	}

	fprintf(stdout, "Compilation successful");
	getchar();		// Hack to make seeing output easier in debug
	return 0;
}