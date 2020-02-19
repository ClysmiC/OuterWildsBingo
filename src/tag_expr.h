// View this file with tab-spacing 4. On GitHub, append ?ts=4 to the URL to do so.

#pragma once

#include "bingo_gen.h"



// Definitions

enum TOKENK
{
	TOKENK_TagLiteral,
	TOKENK_Pipe,
	TOKENK_Amp,
	TOKENK_LParen,
	TOKENK_RParen,

	TOKENK_End,
	TOKENK_Nil = -1
};

struct Token		// tag = token
{
	TOKENK				m_tokenk;
	TAGID				m_tagidLiteral;
};

struct TagExpressionLexer		// tag = telex
{
	const char *		m_pCh;
	int					m_cCh;
	int					m_i;

	// Cached peek token

	Token				m_tokenNextCache;
	int					m_iAfterCacheConsumed;
};



// API

TagExpression * PTagexprCompile(const char * pCh, int cCh);



// Internal

Token TokenPeekNext(TagExpressionLexer * pTelex);
Token TokenConsumeNext (TagExpressionLexer * pTelex);
TagExpression * PTagexprParseLiteral(TagExpressionLexer * pTelex);
TagExpression * PTagexprParseParenGroup(TagExpressionLexer * pTelex);
TagExpression * PTagexprParseBinop(TagExpressionLexer * pTelex, TAGEXPRK tagexprk);
TagExpression * PTagexprParseExpr(TagExpressionLexer * pTelex);
