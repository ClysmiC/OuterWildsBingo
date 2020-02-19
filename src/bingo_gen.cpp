#include "bingo_gen.h"

#include "tag_expr.h"

#include <cstdarg>
#include <stdio.h>

// Global state

char *		g_pChzMan;					// Manifest
int			g_iMan;						// Index
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
	// NOTE (andrews) This function is not intended to advance to the next line. Make sure to call SkipToNextLine when appropriate.

	StringView strvResult;
	strvResult.cCh = 0;
	strvResult.pCh = g_pChzMan + g_iMan;

	if (g_fEof)		return strvResult;

	if (!tryConsumeUntilChar(g_pChzMan, &g_iMan, '\t'))		ErrorAndExit("Expected '\\t'");

	strvResult.cCh = g_pChzMan + g_iMan - strvResult.pCh;

	if (!tryConsumeChar(g_pChzMan, &g_iMan, '\t'))			ErrorAndExit("Expected '\\t'");

	trim(&strvResult);
	return strvResult;
}

void SkipToNextLine()
{
	if (g_fEof)		return;

	static const bool s_fResultOnMatchNull = true;
	Verify(tryConsumeUntilChar(g_pChzMan, &g_iMan, '\n', s_fResultOnMatchNull));

	// NOTE (andrew) Line bump in either case to make a virtual empty line at end of file

	g_nLine++;

	if (g_pChzMan[g_iMan] == '\n')
	{
		g_iMan++;
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

	// NOTE (andrews) Any unknown tag lookup ultimately results in an error. To free the caller of the burden of error
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

	for (int iChCell = 0; iChCell < strvCell.cCh; iChCell++)
	{
		char c = strvCell.pCh[iChCell++];

		if (c == '$')
		{
			if (cDol == 0)
			{
				iChDol0 = iChCell;
			}
			else if (cDol == 1)
			{
				iChDol1 = iChCell;
			}

			cDol++;
		}
	}

	if (cDol == 0)		return false;
	if (cDol != 2)		ErrorAndExit("Could not parse $-expression");

	StringView strvSub;
	strvSub.pCh = strvCell.pCh + iChDol0 + 1;
	strvSub.cCh = 0;

	for (int iCh = iChDol0 + 1; iCh < iChDol1; iCh++)
	{
		char c = strvCell.pCh[iCh++];
		if (c == '/' || iCh == iChDol1 - 1)
		{
			String strCompiled;

			// Init with string before opening $

			init(&strCompiled, strvCell.pCh, iChDol0);

			// Append substituted $-expr

			append(&strCompiled, strvSub.pCh, strvSub.cCh);

			// Add partially compiled string to our list

			append(poAryStrCompiled, strCompiled);

			// Reset strvSub

			strvSub.pCh = strvCell.pCh + iCh;
			strvSub.cCh = 0;
		}
	}

	int cChPostDol1 = strvCell.cCh - (iChDol1 + 1) - 1;
	for (int iStr = 0; iStr < poAryStrCompiled->cItem; iStr++)
	{
		// Append string after closing $ to finish compiling

		append(&(*poAryStrCompiled)[iStr], strvCell.pCh + iChDol1 + 1, cChPostDol1);
	}

	return true;
}

bool FTryCompileAtExpr(const StringView & strvCell, int iDollarCtx, String * poStrCompiled)
{
	// E.g., "PlanetHopping & @BH;ET;DB@" compiles into the following:
	//	iDollarCtx = 0: "PlanetHopping & (BH | ET |DB)"
	//	iDollarCtx = 1: "PlanetHopping & ((BH & ET) | (BH & DB) | (ET & DB))"
	//	iDollarCtx = 2: "PlanetHopping & (BH & ET &DB)"

	int k = iDollarCtx + 1;		// As in "n choose k"

	int iChAt0 = -1;
	int iChAt1 = -1;
	int cAt = 0;

	for (int iChCell = 0; iChCell < strvCell.cCh; iChCell++)
	{
		char c = strvCell.pCh[iChCell++];

		if (c == '@')
		{
			if (cAt == 0)
			{
				iChAt0 = iChCell;
			}
			else if (cAt == 1)
			{
				iChAt1 = iChCell;
			}

			cAt++;
		}
	}

	if (cAt == 0)		return false;
	if (cAt != 2)		ErrorAndExit("Could not parse @-expression");
}

void CreateGeneratedManifestFromRaw()
{
	// Read in raw manifest

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];
	memset((void *)(g_pChzMan), 0, cChManifestMax + 1);
	g_iMan = 0;
	g_nLine = -1;

	{
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
		if (StrvNextCell() != "!!Goals")
		{
			if (g_fEof)		ErrorAndExit("Could not find !!Goals in raw manifest");
			SkipToNextLine();
		}

		SkipToNextLine();		// Skip !!Goals
		SkipToNextLine();		// Skip headers

		if (fwrite(g_pChzMan, 0x1, g_iMan, fileGenerated) != g_iMan)
		{
			g_nLine = -1;
			ErrorAndExit("Failed to write to generated manifest file");
		}

		DynamicArray<Goal> aryGoal;
		init(&aryGoal);

		// Text

		{
			static const char * s_pChzCol = "Text";

			StringView strv = StrvNextCell();
			if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell in \"%s\"", s_pChzCol);

			DynamicArray<String> aryStrSubstituted;
			init(&aryStrSubstituted);
				
			if (FTrySubstituteDollarSlashExpr(strv, &aryStrSubstituted))
			{
				for (int iStr = 0; iStr < aryStrSubstituted.cItem; iStr++)
				{
					Goal * pGoal = appendNew(&aryGoal);
					pGoal->m_strvText.pCh = aryStrSubstituted[iStr].pBuffer;
					pGoal->m_strvText.cCh = aryStrSubstituted[iStr].cChar;
				}
			}
			else
			{
				Goal * pGoal = appendNew(&aryGoal);
				pGoal->m_strvText = strv;
			}
		}

		// Clarification

		{
			StringView strv = StrvNextCell();

			for (int iGoal = 0; iGoal < aryGoal.cItem; iGoal++)
			{
				aryGoal[iGoal].m_strvAlt = strv;
			}
		}

		// Tags
	}
}

int main()
{
	// Init globals

	init(&g_aryTag);
	init(&g_arySynrg);
	init(&g_aryShorthand);
	init(&g_dpaTagexpr);
	g_fEof = false;
	g_fIsGeneratedMan = false;

	CreateGeneratedManifestFromRaw();
	g_fIsGeneratedMan = true;

	// Read in manifest

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];
	memset((void *)(g_pChzMan), 0, cChManifestMax + 1);
	g_iMan = 0;
	g_nLine = -1;

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
			// Text

			struct GoalCtx		// tag = gctx
			{
				// Text

				StringView		m_strvText;		// strv with $-expr substitudet with m_strvN
				StringView		m_strvN;		//	...

				// Other columns

				TagExpression * m_pTagexpr		= nullptr;
				float			m_gDifficulty	= 0;
				float			m_gLength		= 0;
			};

			StringView strvText;
			DynamicArray<GoalCtx> aryGctx;
			init(&aryGctx);

			{
				static const char * s_pChzCol = "Text";

				strvText = StrvNextCell();

				if (strvText.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				// Look for $-expr

				int iCh = 0;
				while (iCh < strvText.cCh)
				{
					char c = strvText.pCh[iCh++];

					if (c == '$')
					{
						int iDolexpr = iCh - 1;
						int cChDolexpr = 1;

						StringView strvN;
						strvN.pCh = strvText.pCh + iCh;
						strvN.cCh = 0;

						while (iCh < strvText.cCh)
						{
							c = strvText.pCh[iCh++];
							cChDolexpr++;

							if (c == '/' || (strvN.cCh > 0 && c == '$'))
							{
								// Validate

								{
									int n;
									int iStrvN = 0;
									if (!tryConsumeInt(strvN.pCh, &iStrvN, &n))		ErrorAndExit("Expected int in $-expression in \"%s\"", s_pChzCol);
									if ((iStrvN - 1) != strvN.cCh)					ErrorAndExit("Expected int in $-expression in \"%s\"", s_pChzCol);
								}

								// Create goal context

								GoalCtx * pGctx = appendNew(&aryGctx);
								pGctx->m_strvN = strvN;

								// Reset strvN

								strvN.pCh = strvText.pCh + iCh;
								strvN.cCh = 0;

								// Build strv for each gctx

								if (c == '$')
								{
									for (int iGctx = 0; iGctx < aryGctx.cItem; iGctx++)
									{
										GoalCtx * pGctx = &aryGctx[iGctx];

										char * pChTextSubstituted = new char[strvText.cCh];
										int cChTextAppended = 0;

										memcpy(
											pChTextSubstituted,
											strvText.pCh,
											iDolexpr
										);
										cChTextAppended = iDolexpr;

										memcpy(
											pChTextSubstituted + cChTextAppended,
											pGctx->m_strvN.pCh,
											pGctx->m_strvN.cCh
										);
										cChTextAppended += pGctx->m_strvN.cCh;

										memcpy(
											pChTextSubstituted + cChTextAppended,
											strvText.pCh + iDolexpr + cChDolexpr,
											strvText.cCh - iDolexpr - cChDolexpr
										);
										cChTextAppended += strvText.cCh - iDolexpr - cChDolexpr;

										AssertInfo(cChTextAppended < strvText.cCh, "We should have cut out dollar signs and the other numbers");
										pChTextSubstituted[cChTextAppended] = '\0'; 

										pGctx->m_strvText.pCh = pChTextSubstituted;
										pGctx->m_strvText.cCh = cChTextAppended;
									}

									break;
								}
							}
							else if (c == '$')
							{
								ErrorAndExit("Could not parse $-expression in \"%s\"", s_pChzCol);
							}
							else
							{
								strvN.cCh++;
							}
						}

						if (iCh == strvText.cCh)		ErrorAndExit("Unclosed $-expression in \"%s\"", s_pChzCol);
					}
				}

				if (aryGctx.cItem == 0)
				{
					// No $-expr

					GoalCtx * pGctx = appendNew(&aryGctx);
					pGctx->m_strvText = strvText;
					pGctx->m_strvN.pCh = nullptr;
					pGctx->m_strvN.cCh = 0;
				}
			}

			// Clarification

			StringView strvClarification = StrvNextCell();

			// TODO (andrew) Audit shorthands in text/clarification

			// Tags

			{
				StringView strv = StrvNextCell();
				if (strv.cCh != 0)
				{
					for (int iGctx; iGctx < aryGctx.cItem; iGctx++)
					{
						aryGctx[iGctx].m_pTagexpr = PTagexprCompile(strv.pCh, strv.cCh, iGctx);
					}
				}
			}

			// Difficulty

			// Length

			SkipToNextLine();
		}
	}

	fprintf(stdout, "Compilation successful");
	getchar();		// Hack to make seeing output easier in debug
	return 0;
}