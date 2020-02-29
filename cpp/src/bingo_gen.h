// View this file with tab-spacing 4. On GitHub, append ?ts=4 to the URL to do so.

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

struct Tag						// tag = tag
{
	TAGID						m_tagid;
	StringView					m_strv;
	DynamicArray<TAGID>			m_aryTagidImplied;
	int							m_cMaxPerRow;
};

struct Synergy					// tag = synrg
{
	TAGID						m_tagid0;
	TAGID						m_tagid1;
	f32							m_gSynrg;
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
	DynamicArray<TAGID>			m_aryTagid;				// Tags associated with this goal
	f32							m_gDifficulty;			// Unit-less, ballparked difficulty. Only has meaning relative to other difficulties.
	f32							m_gLength;				//	... length.

	// TODO: If we add other metatags, make this an array of METATAGID ??

	bool						m_fInlineTracker;		// Should the goal have an inline tracker on the web page?

	f32							m_gScore;				// Score that we export
};