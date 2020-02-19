#include "tag_expr.h"

Token TokenPeekNext(TagExpressionLexer * pTelex)
{
	if (pTelex->m_tokenNextCache.m_tokenk != TOKENK_Nil)		return pTelex->m_tokenNextCache;

	int iStart = pTelex->m_i;
	int i = iStart;

	TOKENK tokenkResult = TOKENK_Nil;
	TAGID tagidLiteralResult = TAGID_Nil;

	while (i < pTelex->m_cCh)
	{
		char c = pTelex->m_pCh[i++];
		if (c == '\0')
		{
			tokenkResult = TOKENK_End;
			goto LTokenCached;
		}

		switch (c)
		{
			case '|':		tokenkResult = TOKENK_Pipe;		goto LTokenCached;
			case '&':		tokenkResult = TOKENK_Amp;		goto LTokenCached;
			case '(':		tokenkResult = TOKENK_LParen;	goto LTokenCached;
			case ')':		tokenkResult = TOKENK_RParen;	goto LTokenCached;

			// BB (andrew) Other white space?

			case ' ':
			case '\t':
				break;

			default:
			{
				if (FIsLegalTagCharacter(c))
				{
					while (true)
					{
						char c = pTelex->m_pCh[i];

						if (FIsLegalTagCharacter(c))
						{
							i++;
						}
						else
						{
							StringView strvTag;
							strvTag.pCh = pTelex->m_pCh + iStart;
							strvTag.cCh = i - iStart;
							trim(&strvTag);

							tokenkResult = TOKENK_TagLiteral;
							tagidLiteralResult = TagidFromStrv(strvTag);
							goto LTokenCached;
						}
					}
				}
				else
				{
					ErrorAndExit("Illegal character in tag expression: '%c'", c);
				}
			} break;
		}
	}

	tokenkResult = TOKENK_End;

LTokenCached:
	pTelex->m_tokenNextCache.m_tokenk = tokenkResult;
	pTelex->m_tokenNextCache.m_tagidLiteral = tagidLiteralResult;
	pTelex->m_iAfterCacheConsumed = i;

	return pTelex->m_tokenNextCache;
}

Token TokenConsumeNext (TagExpressionLexer * pTelex)
{
	Token tokenResult;
	if (pTelex->m_tokenNextCache.m_tokenk == TOKENK_Nil)
	{
		tokenResult = TokenPeekNext(pTelex);
	}
	else
	{
		tokenResult = pTelex->m_tokenNextCache;
	}

	// Advance lexer and clear cache

	pTelex->m_i = pTelex->m_iAfterCacheConsumed;
	pTelex->m_tokenNextCache.m_tokenk = TOKENK_Nil;
	pTelex->m_tokenNextCache.m_tagidLiteral = TAGID_Nil;

	return tokenResult;
}

TagExpression * PTagexprParseLiteral(TagExpressionLexer * pTelex)
{
	Token token = TokenConsumeNext(pTelex);

	if (token.m_tokenk != TOKENK_TagLiteral)		ErrorAndExit("Expected tag literal");

	TagExpression * pTagexprResult = allocate(&g_dpaTagexpr);
	pTagexprResult->m_tagexprk = TAGEXPRK_Literal;
	pTagexprResult->m_tagid = token.m_tagidLiteral;

	return pTagexprResult;
}

TagExpression * PTagexprParseParenGroup(TagExpressionLexer * pTelex)
{
	Token tokenNext = TokenPeekNext(pTelex);

	if (tokenNext.m_tokenk == TOKENK_LParen)
	{
		(void) TokenConsumeNext(pTelex);
		TagExpression * pTagexprResult = PTagexprParseExpr(pTelex);
		
		tokenNext = TokenConsumeNext(pTelex);
		if (tokenNext.m_tokenk != TOKENK_RParen)
		{
			ErrorAndExit("Expected ')' in tag expression");
		}

		return pTagexprResult;
	}
	else
	{
		return PTagexprParseLiteral(pTelex);
	}
}

TagExpression * PTagexprParseBinop(TagExpressionLexer * pTelex, TAGEXPRK tagexprk)
{
	// Hooray for no easy way to make a nested, recursive function in C++ !

	struct PTagexprOneStepLower
	{
		TagExpression * operator()(TagExpressionLexer * pTelex, TAGEXPRK tagexprk)
		{
			if (tagexprk == TAGEXPRK_And)
			{
				return PTagexprParseBinop(pTelex, TAGEXPRK_Or);
			}
			else
			{
				Assert(tagexprk == TAGEXPRK_Or);
				return PTagexprParseParenGroup(pTelex);
			}
		}
	};

	TagExpression * pTagexprResult = PTagexprOneStepLower()(pTelex, tagexprk);
	TOKENK tokenkMatch = (tagexprk == TAGEXPRK_And) ? TOKENK_Amp : TOKENK_Pipe;

	Assert(pTagexprResult);
	Assert(tokenkMatch != TOKENK_Nil);

	while (true)
	{
		Token tokenNext = TokenPeekNext(pTelex);
		if (tokenNext.m_tokenk == tokenkMatch)
		{
			(void) TokenConsumeNext(pTelex);

			TagExpression * pTagexprRhs = PTagexprOneStepLower()(pTelex, tagexprk);

			TagExpression * pTagexprBinop = allocate(&g_dpaTagexpr);
			pTagexprBinop->m_tagexprk = tagexprk;
			pTagexprBinop->m_pTagexprLhs = pTagexprResult;
			pTagexprBinop->m_pTagexprRhs = pTagexprRhs;

			pTagexprResult = pTagexprBinop;
		}
		else
		{
			// Not a binop, just return the expression we have

			return pTagexprResult;
		}
	}
}

TagExpression * PTagexprParseExpr(TagExpressionLexer * pTelex)
{
	return PTagexprParseBinop(pTelex, TAGEXPRK_And);
}

TagExpression * PTagexprCompile(const char * pCh, int cCh, int iDolctx)
{
	Assert(cCh > 0);

	TagExpressionLexer telex;
	telex.m_pCh = pCh;
	telex.m_cCh = cCh;
	telex.m_i = 0;
	telex.m_tokenNextCache.m_tokenk = TOKENK_Nil;
	telex.m_tokenNextCache.m_tagidLiteral = TAGID_Nil;
	telex.m_iAfterCacheConsumed = 0;
	telex.m_iDolctx = iDolctx;

	TagExpression * pTagexprRoot = PTagexprParseExpr(&telex);

	return pTagexprRoot;
}
