#pragma once

#include "als/als.h"


struct StringView				// tag = strv
{
	// TODO: Move this into an als file?

	const char *				m_pCh;
	int							m_cCh;

	bool operator==(const StringView & strvOther) const
	{
		if (m_cCh != strvOther.m_cCh)		return false;
		for (int iCh = 0; iCh < m_cCh; iCh++)
		{
			// NOTE (andrew) Capitalization matters

			if (m_pCh[iCh] != strvOther.m_pCh[iCh])		return false;
		}

		return true;
	}

	bool operator!=(const StringView & strvOther) const
	{
		return !((*this) == strvOther);
	}

	bool operator==(const char * pchz) const
	{
		if (pchz[m_cCh] != '\0' )		return false;

		for (int iCh = 0; iCh < m_cCh; iCh++)
		{
			// NOTE (andrew) Capitalization matters

			if (pchz[iCh] == '\0' )				return false;
			if (m_pCh[iCh] != pchz[iCh])		return false;
		}
	}

	bool operator!=(const char * pchz) const
	{
		return !((*this) == pchz);
	}
};

void Trim(StringView * pStrv)
{
	// Trim start

	while (pStrv->m_cCh > 0)
	{
		// TODO: Better IsWhitespace test

		if (pStrv->m_pCh[0] == ' ' || pStrv->m_pCh[0] == '\t')
		{
			pStrv->m_pCh++;
			pStrv->m_cCh--;
		}
		else
		{
			break;
		}
	}

	// Trim end

	while (pStrv->m_cCh > 0)
	{
		// TODO: Better IsWhitespace test

		if (pStrv->m_pCh[pStrv->m_cCh - 1] == ' ' || pStrv->m_pCh[pStrv->m_cCh - 1] == '\t')
		{
			pStrv->m_cCh--;
		}
		else
		{
			break;
		}
	}
}

enum TAGID
{
	TAGID_Nil = -1
};

enum TAGEXPRK
{
	TAGEXPRK_Literal,
	TAGEXPRK_And,
	TAGEXPRK_Or
};

struct TagExpression			// tag = tagexpr
{
	TAGEXPRK					m_tagexprk;

	union
	{
		TAGID					m_tagid;
		struct
		{
			TagExpression *		m_pTagexprLhs;
			TagExpression *		m_pTagexprRhs;
		};
	};
};

struct Tag						// tag = tag
{
	TAGID						m_tagid;
	StringView					m_strv;
	TagExpression *				m_pTagexprImplied;
	int							m_cMaxPerRow;
};

struct Synergy					// tag = synrg
{
	TAGID						m_tagid0;
	TAGID						m_tagid1;
	f32							m_nSynergy;
};

struct Shorthand				// tag = shorthand
{
	StringView					m_strvShort;
	StringView					m_strvLong;
};

struct Goal
{
	StringView					m_strvText;				// Main displayed text
	StringView					m_strvAlt;				// Clarification / description
	TagExpression *				m_pTagexpr;				// Tags associated with this goal
	f32							m_gDifficulty;			// Unit-less, ballparked difficulty. Only has meaning relative to other difficulties.
	f32							m_gLength;				//	... length.
};