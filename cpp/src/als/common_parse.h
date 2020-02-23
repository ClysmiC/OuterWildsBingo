#pragma once

#ifdef ALS_MACRO
#include "macro_assert.h"
#endif

#ifndef ALS_COMMON_PARSE_StaticAssert
#ifndef ALS_DEBUG
#define ALS_COMMON_PARSE_StaticAssert(X)
#elif defined(ALS_MACRO)
#define ALS_COMMON_PARSE_StaticAssert(x) StaticAssert(x)
#else
// C11 _Static_assert
#define ALS_COMMON_PARSE_StaticAssert(x) _Static_assert(x)
#endif
#endif

#ifndef ALS_COMMON_PARSE_Assert
#ifndef ALS_DEBUG
#define ALS_COMMON_PARSE_Assert(x)
#elif defined(ALS_MACRO)
#define ALS_COMMON_PARSE_Assert(x) Assert(x)
#else
// C assert
#include <assert.h>
#define ALS_COMMON_PARSE_Assert(x) assert(x)
#endif
#endif

#ifndef ALS_COMMON_PARSE_Verify
#ifndef ALS_DEBUG
#define ALS_COMMON_PARSE_Verify(x) x
#else
#define ALS_COMMON_PARSE_Verify(x) ALS_COMMON_PARSE_Assert(x)
#endif
#endif

inline bool tryConsumeInt(const char * pBuffer, int * pI, int *poN)
{
	int i = *pI;
    bool isNeg = false;
    int n = 0;
    int cDigit = 0;

    while (true)
    {
        char c = pBuffer[i++];

        if (c == '-' && cDigit == 0)
        {
            isNeg = !isNeg;
        }
        else if (c >= '0' && c <= '9')
        {
            cDigit++;
            n *= 10;
            n += (c - 48);
        }
        else
        {
            break;
        }
    }

    if (cDigit > 0)
    {
		*pI = i;
        *poN = isNeg ? -n : n;
        return true;
    }
    else
    {
        return false;
    }
}

// NOTE (andrew) "Approx" because the implementation is kind of a hack and probably accumulates error

inline bool tryConsumeFloatApprox(const char * pBuffer, int * pI, float * poF)
{
	int i = *pI;
    bool isNeg = false;
    float f = 0;
    int cDigitPreDecimal = 0;
    int cDigitPostDecimal = 0;
    bool hasDecimal = false;

    while (true)
    {
        char c = pBuffer[i++];

        if (c == '-' && cDigitPreDecimal == 0 && !hasDecimal)
        {
            isNeg = !isNeg;
        }
        else if (c >= '0' && c <= '9')
        {
            if (!hasDecimal)     cDigitPreDecimal++;
            else                 cDigitPostDecimal++;

            // Ignore decimal for now. We will divide it out later.

            f *= 10;
            f += (c - 48);

        }
        else if (c == '.' && !hasDecimal)
        {
            hasDecimal = true;
        }
        else
        {
            break;
        }
    }

    if (cDigitPreDecimal > 0 || cDigitPostDecimal > 0)
    {
        for (int iDigitPost = 0; iDigitPost < cDigitPostDecimal; iDigitPost++)
        {
            f /= 10;
        }

		*pI = i;
        *poF = isNeg ? -f : f;
        return true;
    }
    else
    {
        return false;
    }
}

inline bool tryConsume(const char * pBuffer, int * pI, const char * pchzMatch)
{
    int iBuffer = *pI;
    int iMatch = 0;

    while (true)
    {
        if (pchzMatch[iMatch] == '\0') break;;
        if (pBuffer[iBuffer] != pchzMatch[iMatch]) return false;

        iBuffer++;
        iMatch++;
    }

    *pI += iMatch;
    return true;
}

inline bool tryConsumeChar(const char * pBuffer, int * pI, char * pChMatch, char chMin, char chMax)
{
    char c = pBuffer[*pI];
    if (c >= chMin && c <= chMax)
    {
        *pChMatch = c;
        (*pI)++;
        return true;
    }

    return false;
}

inline bool tryConsumeChar(const char * pBuffer, int * pI, char chMatch)
{
    char c = pBuffer[*pI];
    if (c == chMatch)
    {
        (*pI)++;
        return true;
    }

    return false;
}

// Optional params = The poor man's regex :)

inline bool tryConsumeUntilChar(
	const char * pBuffer,
	int * pI,
	char * poChMatch,
	char chMatch0,
	char chMatch1='\0',
	char chMatch2='\0',
	char chMatch3='\0',
	bool fResultOnMatchNull=false)
{
	int i = *pI;

	while (true)
	{
		char c = pBuffer[i++];

		if (c == '\0')			return fResultOnMatchNull;
		if (c == chMatch0 || c == chMatch1 || c == chMatch2 || c == chMatch3)
		{
			*poChMatch = c;
			*pI = i - 1;		// Consume *until*... so we need to step back one
			return true;
		}
	}
}

inline bool tryConsumeUntilChar(const char * pBuffer, int * pI, char chMatch, bool fResultOnMatchNull=false)
{
	char chMatched;
	return tryConsumeUntilChar(pBuffer, pI, &chMatched, chMatch, '\0', '\0', '\0', fResultOnMatchNull);
}