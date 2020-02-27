// View this file with tab-spacing 4. On GitHub, append ?ts=4 to the URL to do so.

#include "bingo_gen.h"

#include <cstdarg>
#include <float.h>
#include <math.h>
#include <stdio.h>

// Global state

char *		g_pChzMan;					// Manifest
int			g_iChMan;					// Index
int			g_nLine;					// Line #
bool		g_fEof;						// Have we hit the end of current file

DynamicArray<Tag>						g_aryTag;
DynamicArray<Synergy>					g_arySynrg;
DynamicArray<Shorthand>					g_aryShorthand;
DynamicArray<Goal>						g_aryGoal;



// Implementation

void ErrorAndExit(const char * errFormat, ...)
{
	if (g_nLine >= 0)
	{
		fprintf(stderr, "[Error on line %d]: ", g_nLine);
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

void ErrorAndExitStrvHack(const char * errFormat, const StringView & strv0)
{
	// Convenience hack to use %s like printf with a non-zero terminated string.

	String str0;
	init(&str0, strv0.pCh, strv0.cCh);
	ErrorAndExit(errFormat, str0.pBuffer);
}

void ErrorAndExitStrvHack(const char * errFormat, const StringView & strv0, const StringView & strv1)
{
	// Convenience hack to use %s like printf with a non-zero terminated string.

	String str0;
	init(&str0, strv0.pCh, strv0.cCh);

	String str1;
	init(&str1, strv1.pCh, strv1.cCh);

	ErrorAndExit(errFormat, str0.pBuffer, str1.pBuffer);
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

TAGID TagidFromStrv(const StringView & strv, bool fErrorOnNotFound=true)
{
	for (int iTag = 0; iTag < g_aryTag.cItem; iTag++)
	{
		if (strv == g_aryTag[iTag].m_strv)		return g_aryTag[iTag].m_tagid;
	}

	// NOTE (andrew) Any unknown tag lookup ultimately results in an error. To free the caller of the burden of error
	//	checking, we report it right here.

	if (fErrorOnNotFound)
	{
		char * pChzStrvCopy = new char[strv.cCh + 1];
		memcpy(pChzStrvCopy, strv.pCh, strv.cCh);
		pChzStrvCopy[strv.cCh] = '\0';

		ErrorAndExit("Unknown tag: %s", pChzStrvCopy);
	}

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

void AuditShorthands(StringView strv)
{
	int iChLBrack = -1;

	for (int iCh = 0; iCh < strv.cCh; iCh++)
	{
		char c = strv.pCh[iCh];

		if (c == '[')
		{
			if (iChLBrack != -1)		ErrorAndExit("Shorthand brackets must be balanced and un-nested");

			iChLBrack = iCh;
		}
		else if (c == ']')
		{
			if (iChLBrack == -1)		ErrorAndExit("Shorthand brackets must be balanced and un-nested");

			StringView strvShort;
			strvShort.pCh = strv.pCh + iChLBrack + 1;
			strvShort.cCh = iCh - (iChLBrack + 1);

			bool fFound = false;
			for (int iShorthand = 0; iShorthand < g_aryShorthand.cItem; iShorthand++)
			{
				if (g_aryShorthand[iShorthand].m_strvShort == strvShort)
				{
					fFound = true;
					break;
				}
			}

			if (!fFound)		ErrorAndExitStrvHack("Unknown shorthand: %s", strvShort);

			iChLBrack = -1;
		}
	}

	if (iChLBrack != -1)		ErrorAndExit("Shorthand brackets must be balanced and unnested");
}

void NextCellVerify(const char * pChzExpected)
{
	if (StrvNextCell() != pChzExpected)		ErrorAndExit("Expected \"%s\"", pChzExpected);
}

void CompileManifest()
{
	// Init needed globals

	init(&g_aryShorthand);
	init(&g_aryTag);
	init(&g_arySynrg);
	init(&g_aryGoal);

	g_pChzMan = nullptr;
	g_iChMan = 0;
	g_nLine = -1;
	g_fEof = false;

	int cBManifestMax = 1024 * 1024;				// Max = 1MB
	int cChManifestMax = cBManifestMax - 1;

	// NOTE (andrew) Since this is a one-off tool with fairly limited scope, we allocate memory when we need it and never bother to free it.

	g_pChzMan = new char[cChManifestMax + 1];
	memset((void *)(g_pChzMan), 0, cChManifestMax + 1);

	{
		// TODO: Allow command line override (?)

		char * pChzManifestFile = "../res/manifest.tsv";

		FILE * file = fopen(pChzManifestFile, "rb");
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

	g_nLine = 1;

	// Shorthands

	{
		// Verify header

		{
			NextCellVerify("!!Shorthands");
			SkipToNextLine();
			NextCellVerify("Short");
			NextCellVerify("Long");
			SkipToNextLine();
		}

		// Parse shorthand list

		while (true)
		{
			// Short

			StringView strvShort;

			{
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
				strvLong = StrvNextCell();
				if (strvLong.cCh == 0)		ErrorAndExit("Illegal empty cell");
			}

			Shorthand * pShorthand = appendNew(&g_aryShorthand);
			pShorthand->m_strvShort = strvShort;
			pShorthand->m_strvLong = strvLong;

			SkipToNextLine();
		}
	}

	// Tags

	{
		// Verify header

		{
			NextCellVerify("!!Tags");
			SkipToNextLine();
			NextCellVerify("Tag");
			NextCellVerify("Implies");
			NextCellVerify("MaxPerRow");
			NextCellVerify("SelfSynergy");
			SkipToNextLine();
		}

		// Parse tag list

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

				trim(&strv);

				for (int iCh = 0; iCh < strv.cCh; iCh++)
				{
					if (!FIsLegalTagCharacter(strv.pCh[iCh]))		ErrorAndExit("Tags may only contain letters and underscores");
				}

				static const bool s_fErrorOnNotFound = false;
				if (TagidFromStrv(strv, s_fErrorOnNotFound) != TAGID_Nil)
				{
					String str;
					init(&str, strv.pCh, strv.cCh);
					ErrorAndExit("Duplicate tag: %s", str.pBuffer);
				}

				pTag = appendNew(&g_aryTag);
				pTag->m_strv = strv;
				pTag->m_tagid = TAGID(g_aryTag.cItem - 1);
			}

			// Implies

			{
				StringView strvCell = StrvNextCell();
				trim(&strvCell);
				
				init(&pTag->m_aryTagidImplied);

				if (strvCell.cCh > 0)
				{
					int iChPostPrevComma = 0;
					for (int iCh = 0; iCh <= strvCell.cCh; iCh++)
					{
						char c = (iCh < strvCell.cCh) ? strvCell.pCh[iCh] : ',';

						if (c == ',')
						{
							StringView strvTagImplied;
							strvTagImplied.pCh = strvCell.pCh + iChPostPrevComma;
							strvTagImplied.cCh = iCh - iChPostPrevComma;

							trim(&strvTagImplied);

							TAGID tagidImplied = TagidFromStrv(strvTagImplied);

							DynamicArray<TAGID> aryTagidProcess;
							init(&aryTagidProcess);
							append(&aryTagidProcess, tagidImplied);

							while (aryTagidProcess.cItem > 0)
							{
								TAGID tagidProcess = aryTagidProcess[0];
								Assert(tagidProcess != TAGID_Nil);

								if (tagidProcess == pTag->m_tagid)
								{
									ErrorAndExitStrvHack("Tag cannot imply itself: %s", pTag->m_strv);
								}

								Tag * pTagProcess = &g_aryTag[tagidProcess];
								if (indexOf(pTag->m_aryTagidImplied, tagidProcess) == -1)
								{
									append(&pTag->m_aryTagidImplied, tagidProcess);

									for (int iTagidProcessImplied = 0; iTagidProcessImplied < pTagProcess->m_aryTagidImplied.cItem; iTagidProcessImplied++)
									{
										append(&aryTagidProcess, pTagProcess->m_aryTagidImplied[iTagidProcessImplied]);
									}
								}

								unorderedRemove(&aryTagidProcess, 0);
							}

							iChPostPrevComma = iCh + 1;
						}
					}
				}
			}

			// Max per row

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)													ErrorAndExit("Illegal empty cell");

				int iChStrv = 0;
				if (!tryConsumeInt(strv.pCh, &iChStrv, &pTag->m_cMaxPerRow))		ErrorAndExit("Expected integer");
				if (pTag->m_cMaxPerRow <= 0)										ErrorAndExit("Expected number > 0");
			}

			// Self synergy

			{
				StringView strv = StrvNextCell();

				if (strv.cCh == 0)													ErrorAndExit("Illegal empty cell");

				float gSynrg;
				int iChStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iChStrv, &gSynrg))			ErrorAndExit("Expected float");

				if (!FloatEq(gSynrg, 0, 0.01))
				{
					if (pTag->m_cMaxPerRow == 1)									ErrorAndExit("Expected no self-synergy for goal with max per row of 1");

					Synergy * pSynrg = appendNew(&g_arySynrg);
					pSynrg->m_tagid0 = pTag->m_tagid;
					pSynrg->m_tagid1 = pTag->m_tagid;
					pSynrg->m_gSynrg = gSynrg;
				}
			}

			SkipToNextLine();
		}
	}

	// Synergies

	{
		// Verify header

		{
			NextCellVerify("!!Synergies");
			SkipToNextLine();
			NextCellVerify("Tag0");
			NextCellVerify("Tag1");
			NextCellVerify("Synergy");
			SkipToNextLine();
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
				StringView strvCell = StrvNextCell();
				if (strvCell.cCh == 0)		ErrorAndExit("Illegal empty cell");

				int iChPostPrevComma = 0;
				for (int iCh = 0; iCh <= strvCell.cCh; iCh++)
				{
					char c = (iCh < strvCell.cCh) ? strvCell.pCh[iCh] : ',';

					if (c == ',')
					{
						StringView strvTag1;
						strvTag1.pCh = strvCell.pCh + iChPostPrevComma;
						strvTag1.cCh = iCh - iChPostPrevComma;

						trim(&strvTag1);

						bool fOneWay = false;
						if (strvTag1.pCh[0] == '[' && strvTag1.pCh[strvTag1.cCh - 1] == ']')
						{
							fOneWay = true;
							strvTag1.pCh += 1;		// Strip []
							strvTag1.cCh -= 2;
						}

						auto AddSynrg = [](TAGID tagid0, TAGID tagid1)
						{
							// NOTE (andrew) Actual synergy value gets filled in once we parse the next cell

							if (tagid0 == tagid1)
							{
								ErrorAndExitStrvHack("Self synergy of %s cannot be listed in !!Synergies (should be in !!Tags)", g_aryTag[tagid0].m_strv);
							}

							for (int iSynrg = 0; iSynrg < g_arySynrg.cItem; iSynrg++)
							{
								Synergy * pSynrg = &g_arySynrg[iSynrg];
								if (pSynrg->m_tagid0 == tagid0 && pSynrg->m_tagid1 == tagid1)
								{
									ErrorAndExitStrvHack(
										"Duplicate synergy %s -> %s",
										g_aryTag[pSynrg->m_tagid0].m_strv,
										g_aryTag[pSynrg->m_tagid1].m_strv
									);
								}
							}

							Synergy * pSynrg = appendNew(&g_arySynrg);
							pSynrg->m_tagid0 = tagid0;
							pSynrg->m_tagid1 = tagid1;
						};

						TAGID tagid1 = TagidFromStrv(strvTag1);
						AddSynrg(tagid0, tagid1);
						cSynrgAdded++;

						if (!fOneWay)
						{
							AddSynrg(tagid1, tagid0);
							cSynrgAdded++;
						}

						iChPostPrevComma = iCh + 1;
					}
				}
			}

			// Synergy

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)											ErrorAndExit("Illegal empty cell");

				float gSynrg = 0;
				int iStrv = 0;
				if (!tryConsumeFloatApprox(strv.pCh, &iStrv, &gSynrg))		ErrorAndExit("Expected float");

				if (!FloatEq(gSynrg, 0, 0.01))
				{
					// Patch up the synergies we just added with their correct values

					for (int iSynrgAdded = 0; iSynrgAdded < cSynrgAdded; iSynrgAdded++)
					{
						g_arySynrg[g_arySynrg.cItem - iSynrgAdded - 1].m_gSynrg = gSynrg;
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

	// Goals

	{
		// Verify header

		{
			NextCellVerify("!!Goals");
			SkipToNextLine();
			NextCellVerify("Text");
			NextCellVerify("Clarification");
			NextCellVerify("Tags");
			NextCellVerify("Difficulty");
			NextCellVerify("Length");
			SkipToNextLine();
		}

		// Parse goal list

		while (true)
		{
			struct GoalCtx		// tag = gctx
			{
				StringView		m_strvText;
				StringView		m_strvAlt;
				StringView		m_strvTags;
				StringView		m_strvDifficulty;
				StringView		m_strvLength;
			};

			DynamicArray<GoalCtx> aryGctx;
			init(&aryGctx);

			// Text

			{
				StringView strv = StrvNextCell();

				if (strv.cCh == 0)
				{
					// Assume an empty first cell is an entirely empty line

					SkipToNextLine();
					break;
				}

				if (startsWith(strv, "[TODO]") || startsWith(strv, "[COMMENT]"))
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
						StringView strvSubstituted;
						strvSubstituted.pCh = aryStrSubstituted[iStr].pBuffer;
						strvSubstituted.cCh = aryStrSubstituted[iStr].cChar;

						AuditShorthands(strvSubstituted);

						GoalCtx * pGctx = appendNew(&aryGctx);
						pGctx->m_strvText = strvSubstituted;
					}
				}
				else
				{
					AuditShorthands(strv);

					GoalCtx * pGctx = appendNew(&aryGctx);
					pGctx->m_strvText = strv;
				}
			}

			// Clarification

			{
				StringView strv = StrvNextCell();
				AuditShorthands(strv);

				for (int iGctx = 0; iGctx < aryGctx.cItem; iGctx++)
				{
					aryGctx[iGctx].m_strvAlt = strv;
				}
			}

			// Tags

			{
				StringView strv = StrvNextCell();

				DynamicArray<String> aryStrSubstituted;
				init(&aryStrSubstituted);
		
				// $-expr

				if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
				{
					if (aryStrSubstituted.cItem != aryGctx.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

					for (int iGctx = 0; iGctx < aryGctx.cItem; iGctx++)
					{
						GoalCtx * pGctx = &aryGctx[iGctx];
						pGctx->m_strvTags.pCh = aryStrSubstituted[iGctx].pBuffer;
						pGctx->m_strvTags.cCh = aryStrSubstituted[iGctx].cChar;
					}
				}
				else
				{
					for (int iGctx = 0; iGctx < aryGctx.cItem; iGctx++)
					{
						GoalCtx * pGctx = &aryGctx[iGctx];
						pGctx->m_strvTags = strv;
					}
				}
			}

			// Difficulty

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell");

				DynamicArray<String> aryStrSubstituted;
				init(&aryStrSubstituted);
				
				if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
				{
					if (aryStrSubstituted.cItem != aryGctx.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

					for (int iGoal = 0; iGoal < aryGctx.cItem; iGoal++)
					{
						GoalCtx * pGctx = &aryGctx[iGoal];
						pGctx->m_strvDifficulty.pCh = aryStrSubstituted[iGoal].pBuffer;
						pGctx->m_strvDifficulty.cCh = aryStrSubstituted[iGoal].cChar;
					}
				}
				else
				{
					for (int iGoal = 0; iGoal < aryGctx.cItem; iGoal++)
					{
						GoalCtx * pGctx = &aryGctx[iGoal];
						pGctx->m_strvDifficulty = strv;
					}
				}
			}

			// Length

			{
				StringView strv = StrvNextCell();
				if (strv.cCh == 0)		ErrorAndExit("Illegal empty cell");

				DynamicArray<String> aryStrSubstituted;
				init(&aryStrSubstituted);
				
				if (FTryCompileDollarExpr(strv, &aryStrSubstituted))
				{
					if (aryStrSubstituted.cItem != aryGctx.cItem)		ErrorAndExit("Inconsistent number of goals in $-expressions");

					for (int iGoal = 0; iGoal < aryGctx.cItem; iGoal++)
					{
						GoalCtx * pGctx = &aryGctx[iGoal];
						pGctx->m_strvLength.pCh = aryStrSubstituted[iGoal].pBuffer;
						pGctx->m_strvLength.cCh = aryStrSubstituted[iGoal].cChar;
					}
				}
				else
				{
					for (int iGoal = 0; iGoal < aryGctx.cItem; iGoal++)
					{
						GoalCtx * pGctx = &aryGctx[iGoal];
						pGctx->m_strvLength = strv;
					}
				}
			}

			// Generate actual goals now that we have substituted $-expressions

			for (int iCtx = 0; iCtx < aryGctx.cItem; iCtx++)
			{
				GoalCtx * pGctx = &aryGctx[iCtx];

				// Text

				Goal * pGoal = appendNew(&g_aryGoal);
				pGoal->m_strvText = pGctx->m_strvText;
				pGoal->m_strvAlt = pGctx->m_strvAlt;

				// Tags

				init(&pGoal->m_aryTagid);
				StringView strvTags = pGctx->m_strvTags;

				if (strvTags.cCh > 0)
				{
					int iChPostPrevComma = 0;
					for (int iCh = 0; iCh <= strvTags.cCh; iCh++)
					{
						char c = (iCh < strvTags.cCh) ? strvTags.pCh[iCh] : ',';

						if (c == ',')
						{
							StringView strvTag;
							strvTag.pCh = strvTags.pCh + iChPostPrevComma;
							strvTag.cCh = iCh - iChPostPrevComma;

							trim(&strvTag);

							// Add tag

							TAGID tagid = TagidFromStrv(strvTag);
							Tag * pTag = &g_aryTag[tagid];
							if (indexOf(pGoal->m_aryTagid, tagid) != -1)
							{
								ErrorAndExitStrvHack("Goal \"%s\" includes tag %s multiple times (possibly indirectly)", pGoal->m_strvText, pTag->m_strv);
							}

							append(&pGoal->m_aryTagid, tagid);

							// Add any tags that it implies

							for (int iTagidImplied = 0; iTagidImplied < pTag->m_aryTagidImplied.cItem; iTagidImplied++)
							{
								TAGID tagidImplied = pTag->m_aryTagidImplied[iTagidImplied];
								Tag * pTagImplied = &g_aryTag[tagidImplied];
								if (indexOf(pGoal->m_aryTagid, tagidImplied) != -1)
								{
									ErrorAndExitStrvHack("Goal \"%s\" includes tag %s multiple times (possibly indirectly)", pGoal->m_strvText, pTagImplied->m_strv);
								}

								append(&pGoal->m_aryTagid, tagidImplied);
							}

							iChPostPrevComma = iCh + 1;
						}
					}
				}

				// Difficulty

				{
					Assert(pGctx->m_strvDifficulty.cCh > 0);

					int iChStrv = 0;
					if (!tryConsumeFloatApprox(pGctx->m_strvDifficulty.pCh, &iChStrv, &pGoal->m_gDifficulty))		ErrorAndExit("Expected float");
					if (pGoal->m_gDifficulty <= 0)																	ErrorAndExit("Expected number > 0");
				}

				// Length

				{
					Assert(pGctx->m_strvLength.cCh > 0);

					int iChStrv = 0;
					if (!tryConsumeFloatApprox(pGctx->m_strvLength.pCh, &iChStrv, &pGoal->m_gLength))		ErrorAndExit("Expected float");
					if (pGoal->m_gLength<= 0)																ErrorAndExit("Expected number > 0");
				}
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
}

void DumpToJson()
{
	auto AppendTabs = [](String * pStr, int cTab)
	{
		for (int iTab = 0; iTab < cTab; iTab++)
		{
			append(pStr, "\t");
		}
	};

	auto PChzEscapedFromStrv = [](StringView strv)
	{
		String str;
		init(&str);

		for (int iCh = 0; iCh < strv.cCh; iCh++)
		{
			// TODO: Add any other escape characters that I need

			char c = strv.pCh[iCh];
			if (c == '"')
			{
				append(&str, '\\');
			}

			append(&str, c);
		}

		return str.pBuffer;
	};

	auto PChzFromInt = [](int n)
	{
		char * pChzBuffer = new char[16];
		int nPrinted = sprintf(pChzBuffer, "%d", n);

		if (nPrinted >= 16)		ErrorAndExit("Integer cannot exceed 15 digits");
		return pChzBuffer;
	};

	auto PChzFromFloat = [](float f)
	{
		char * pChzBuffer = new char[16];
		int nPrinted = sprintf(pChzBuffer, "%0.4f", f);

		if (nPrinted >= 16)		ErrorAndExit("Float cannot exceed 15 digits");
		return pChzBuffer;
	};

	auto PChzShorthands = [AppendTabs, PChzEscapedFromStrv, PChzFromInt, PChzFromFloat](int cTab)
	{
		String str;
		init(&str);

		for (int iShorthand = 0; iShorthand < g_aryShorthand.cItem; iShorthand++)
		{
			Shorthand * pShorthand = &g_aryShorthand[iShorthand];

			if (iShorthand != 0)
			{
				AppendTabs(&str, cTab);
			}

			if (iShorthand == 0)		append(&str, "  ");
			else						append(&str, ", ");

			append(&str, "{ ");

			append(&str, "\"short\": \"");
			append(&str, PChzEscapedFromStrv(pShorthand->m_strvShort));
			append(&str, "\", \"long\": \"");
			append(&str, PChzEscapedFromStrv(pShorthand->m_strvLong));
			append(&str, "\"");

			append(&str, " }");

			if (iShorthand != g_aryShorthand.cItem - 1)		append(&str, "\n");
		}

		return str.pBuffer;
	};

	auto PChzTags = [AppendTabs, PChzEscapedFromStrv, PChzFromInt, PChzFromFloat](int cTab)
	{
		String str;
		init(&str);

		for (int iTag = 0; iTag < g_aryTag.cItem; iTag++)
		{
			Tag * pTag = &g_aryTag[iTag];

			if (iTag != 0)
			{
				AppendTabs(&str, cTab);
			}

			if (iTag == 0)		append(&str, "  ");
			else				append(&str, ", ");

			append(&str, "{ ");

			append(&str, "\"maxPerRow\": ");
			append(&str, PChzFromInt(pTag->m_cMaxPerRow));
			append(&str, ", \"synergies\": [");

			int cSynrgPrinted = 0;
			for (int iSynrg = 0; iSynrg < g_arySynrg.cItem; iSynrg++)
			{
				Synergy * pSynrg = &g_arySynrg[iSynrg];

				if (pSynrg->m_tagid0 != pTag->m_tagid)		continue;

				if (cSynrgPrinted == 0)		append(&str, " ");
				else						append(&str, ", ");

				append(&str, "{ ");
				append(&str, "\"tagOther\": ");
				append(&str, PChzFromInt(pSynrg->m_tagid1));
				append(&str, ", \"synergy\": ");
				append(&str, PChzFromFloat(pSynrg->m_gSynrg));
				append(&str, "}");

				cSynrgPrinted++;
			}

			if (cSynrgPrinted > 0)		append(&str, " ");

			append(&str, "] }");

			if (iTag != g_aryTag.cItem - 1)		append(&str, "\n");
		}

		return str.pBuffer;
	};

	struct GoalStats		// tag = gstats
	{
		float		m_gDifficultyAvg;
		float		m_gDifficultyStddev;
		float		m_gLengthAvg;
		float		m_gLengthStddev;
		float		m_gScoreAvg;
		float		m_gScoreStddev;
		float		m_gPairwiseGoalSynergyAvg;
	};

	auto PChzGoals = [AppendTabs, PChzEscapedFromStrv, PChzFromInt, PChzFromFloat](int cTab)
	{
		String str;
		init(&str);

		for (int iGoal = 0; iGoal < g_aryGoal.cItem; iGoal++)
		{
			Goal * pGoal = &g_aryGoal[iGoal];

			if (iGoal != 0)
			{
				AppendTabs(&str, cTab);
			}

			append(&str, "{\n");

			AppendTabs(&str, cTab + 1);
			append(&str, "\"text\": \"");
			append(&str, PChzEscapedFromStrv(pGoal->m_strvText));
			append(&str, "\",\n");

			AppendTabs(&str, cTab + 1);
			append(&str, "\"score\": ");
			append(&str, PChzFromFloat(pGoal->m_gScore));
			append(&str, ",\n");

			AppendTabs(&str, cTab + 1);
			append(&str, "\"tags\": [");
			
			for (int iTag = 0; iTag < pGoal->m_aryTagid.cItem; iTag++)
			{
				TAGID tagid = pGoal->m_aryTagid[iTag];

				if (iTag == 0)		append(&str, " ");
				else				append(&str, ", ");

				append(&str, PChzFromInt(tagid));

				if (iTag == pGoal->m_aryTagid.cItem - 1)		append(&str, " ");
			}
			append(&str, "],\n");

			AppendTabs(&str, cTab + 1);
			append(&str, "\"alt\": \"");
			append(&str, PChzEscapedFromStrv(pGoal->m_strvAlt));
			append(&str, "\"\n");

			AppendTabs(&str, cTab);
			append(&str, "}");

			if (iGoal != g_aryGoal.cItem - 1)		append(&str, ",\n");
		}

		return str.pBuffer;
	};

	auto GstatsComputeAndFinalizeGoals = [](DynamicArray<Goal> * pAryGoal)
	{
		GoalStats gstats;

		// Compute raw averages

		{
			float gDifficultySum = 0;
			float gLengthSum = 0;

			for (int iGoal = 0; iGoal < pAryGoal->cItem; iGoal++)
			{
				gDifficultySum += (*pAryGoal)[iGoal].m_gDifficulty;
				gLengthSum += (*pAryGoal)[iGoal].m_gLength;
			}

			gstats.m_gDifficultyAvg = gDifficultySum / pAryGoal->cItem;
			gstats.m_gLengthAvg = gLengthSum / pAryGoal->cItem;
		}

		// Compute raw stddevs

		{
			float gDifficultyStddev = 0;
			float gLengthStddev = 0;
			{
				for (int iGoal = 0; iGoal < pAryGoal->cItem; iGoal++)
				{
					float dDifficulty = (*pAryGoal)[iGoal].m_gDifficulty - gstats.m_gDifficultyAvg;
					gDifficultyStddev += dDifficulty * dDifficulty;

					float dLength = (*pAryGoal)[iGoal].m_gLength - gstats.m_gLengthAvg;
					gLengthStddev += dLength * dLength;
				}

				gDifficultyStddev /= pAryGoal->cItem;
				gDifficultyStddev = sqrtf(gDifficultyStddev);

				gLengthStddev /= pAryGoal->cItem;
				gLengthStddev = sqrtf(gLengthStddev);
			}

			gstats.m_gDifficultyStddev = gDifficultyStddev;
			gstats.m_gLengthStddev = gLengthStddev;
		}


		// Finalize goal scores and compute avg

		{
			float gScoreSum = 0;
			for (int iGoal = 0; iGoal < pAryGoal->cItem; iGoal++)
			{
				Goal * pGoal = &(*pAryGoal)[iGoal];
				float dSigmaDifficulty = (pGoal->m_gDifficulty - gstats.m_gDifficultyAvg) / gstats.m_gDifficultyStddev;

				// NOTE (andrew) The idea is that the "score" is a fusion of the length and the difficulty of a goal. The actual formula
				//	for combining them is pretty arbitrary. Basically, I want to add 1 to the length per sigma that the difficulty is above
				//	some minimum threshold that is effectively "zero" difficulty. I don't want to use the minimum difficulty to prevent against
				//	outliers, so the baseline is just a negative sigma that will cover almost every goal. Again, it is fairly arbitrary and
				//	is just another knob I can tweak to balance the board.

				static const float s_dSigmaBaseline = -3;
				pGoal->m_gScore = pGoal->m_gLength + (dSigmaDifficulty - s_dSigmaBaseline);
				gScoreSum += pGoal->m_gScore;
			}

			gstats.m_gScoreAvg = gScoreSum / pAryGoal->cItem;
		}

		// Compute score stddev

		{
			float gScoreStddev = 0;
			{
				for (int iGoal = 0; iGoal < pAryGoal->cItem; iGoal++)
				{
					float dScore = (*pAryGoal)[iGoal].m_gScore - gstats.m_gScoreAvg;
					gScoreStddev += dScore * dScore;
				}

				gScoreStddev /= pAryGoal->cItem;
				gScoreStddev = sqrtf(gScoreStddev);
			}

			gstats.m_gScoreStddev = gScoreStddev;
		}

		// Compute average pairwise synergy

		{
			float gSynergyTotal = 0;
			int cSynergyValid = 0;

			for (int iGoal = 0; iGoal < g_aryGoal.cItem; iGoal++)
			{
				for (int iGoalOther = iGoal + 1; iGoalOther < g_aryGoal.cItem; iGoalOther++)
				{
					const Goal & goal = g_aryGoal[iGoal];
					const Goal & goalOther = g_aryGoal[iGoalOther];

					// NOTE (andrew) Start at -FLT_MAX so if goals are anti-synergistic (i.e., -synergy) we will
					//	account for that. But if they have no synergies we will swap in a 0.

					float gMaxSynrgFound = -FLT_MAX;

					for (int iTagid = 0; iTagid < goal.m_aryTagid.cItem; iTagid++)
					{
						TAGID tagid = goal.m_aryTagid[iTagid];

						for (int iTagidOther = 0; iTagidOther < goalOther.m_aryTagid.cItem; iTagidOther++)
						{
							TAGID tagidOther = goalOther.m_aryTagid[iTagidOther];

							if (tagid == tagidOther && g_aryTag[tagid].m_cMaxPerRow == 1)		goto LNextGoalOther;

							// @Slow Search through every synergy to find if we have one with both tagid's

							Synergy * pSynrg = nullptr;
							{
								for (int iSynrg = 0; iSynrg < g_arySynrg.cItem; iSynrg++)
								{
									Synergy * pSynrgCandidate = &g_arySynrg[iSynrg];

									if ((pSynrgCandidate->m_tagid0 == tagid && pSynrgCandidate->m_tagid1 == tagidOther) ||
										(pSynrgCandidate->m_tagid0 == tagidOther && pSynrgCandidate->m_tagid1 == tagid))
									{
										pSynrg = pSynrgCandidate;;
										break;
									}
								}
							}

							if (pSynrg)		gMaxSynrgFound = Max(gMaxSynrgFound, pSynrg->m_gSynrg);
						}
					}

					gMaxSynrgFound = (gMaxSynrgFound == -FLT_MAX) ? 0 : gMaxSynrgFound;
					gSynergyTotal += gMaxSynrgFound;
					cSynergyValid++;

				LNextGoalOther:
					continue;
				}
			}

			gstats.m_gPairwiseGoalSynergyAvg = gSynergyTotal / cSynergyValid;
		}

		return gstats;
	};

	FILE * file = fopen("../res/compiled.json", "wb");
	Defer (if (file) fclose(file));
	if (!file)		ErrorAndExit("Failed to create compiled JSON file");

	GoalStats gstats = GstatsComputeAndFinalizeGoals(&g_aryGoal);

	int nResult = fprintf(file,
R"jsonblob({
	"shorthands":
	[
		%s
	],

	"tags":
	[
		%s
	],

	"goals":
	[
		%s
	],

	"goalScoreAvg": %f,
	"goalScoreStddev": %f,
	"pairwiseGoalSynergyAvg": %f
})jsonblob",
	PChzShorthands(2),
	PChzTags(2),
	PChzGoals(2),
	gstats.m_gScoreAvg,
	gstats.m_gScoreStddev,
	gstats.m_gPairwiseGoalSynergyAvg);

	if (nResult < 0)		ErrorAndExit("Failed to write to compiled JSON file");
}

void CopyToSite()
{
	FILE * fileSrc = fopen("../res/compiled.json", "rb");
	FILE * fileDst = fopen("../site/manifest/manifest.js", "wb");

	Defer(
		fclose(fileSrc);
		fclose(fileDst);
	);

	if (!fileSrc || !fileDst)		ErrorAndExit("Failed to copy compiled.json to site directory");

	bool fFail = false;

	fFail = fFail || (fputs("// Hello curious Hearthian! The contents of this file are auto-generated from a master spreadsheet that I use to define goals and their various synergies. If you are interested in helping contribute goal ideas or balance suggestions, please contact me @ClysmiC11 on Twitter!\n\n", fileDst) < 0);
	fFail = fFail || (fputs("let manifest = ", fileDst) < 0);

	char c;
	while ((c = fgetc(fileSrc)) != EOF)
	{
		fFail = fFail || (fputc(c, fileDst) < 0);
	}

	fFail = fFail || (fputs(";", fileDst) < 0);
	if (fFail)		ErrorAndExit("Failed to copy compiled.json to site directory");
}

int main()
{
	CompileManifest();
	fprintf(stdout, "Manifest compiled...\n");

	DumpToJson();
	fprintf(stdout, "Dumped to JSON...\n");

	CopyToSite();
	fprintf(stdout, "Copied to site directory...\n");

	fprintf(stdout, "[Complete]");

	getchar();		// Hack to make seeing output easier in debug
	return 0;
}