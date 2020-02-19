#pragma once

#include "als/als.h"



// Definitions

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
	f32							m_nSynrg;
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



// Global state

extern char *		g_pChzMan;			// Manifest
extern int			g_iMan;				// Index
extern int			g_nLine;			// Line #

extern DynamicArray<Tag>						g_aryTag;
extern DynamicArray<Synergy>					g_arySynrg;
extern DynamicArray<Shorthand>					g_aryShorthand;
extern DynamicPoolAllocator<TagExpression>		g_dpaTagexpr;



// API

void ErrorAndExit(const char * errFormat, ...);
bool FIsLegalTagCharacter(char c);



// Internal

StringView StrvNextCell();
void SkipToNextLine();
TAGID TagidFromStrv(const StringView & strv);



// main

int main();