#include "stripped_sqlite_int.h"

struct StrAccum {
	void *db;       /* Optional database for lookaside.  Can be NULL */
	char *zText;    /* The string collected so far */
	u32 nAlloc;     /* Amount of space allocated in zText */
	u32 mxAlloc;    /* Maximum allowed allocation.  0 for no malloc usage */
	u32 nChar;      /* Length of the string so far */
	u8 accError;    /* STRACCUM_NOMEM or STRACCUM_TOOBIG */
	u8 printfFlags; /* SQLITE_PRINTF flags below */
};
#define STRACCUM_NOMEM         1
#define STRACCUM_TOOBIG        2
#define SQLITE_PRINTF_INTERNAL 0x01 /* Internal-use-only converters allowed */
#define SQLITE_PRINTF_SQLFUNC  0x02 /* SQL function arguments to VXPrintf */
#define SQLITE_PRINTF_MALLOCED 0x04 /* True if xText is allocated space */

#define isMalloced(X) (((X)->printfFlags & SQLITE_PRINTF_MALLOCED) != 0)

typedef struct StrAccum StrAccum;

void sqlite3StrAccumAppend(StrAccum *p, const char *z, int N);
void sqlite3AppendChar(StrAccum *p, int N, char c);
void sqlite3StrAccumAppendAll(StrAccum *p, const char *z);
void sqlite3StrAccumReset(StrAccum *p);

/*
** The "printf" code that follows dates from the 1980's.  It is in
** the public domain.
**
**************************************************************************
**
** This file contains code for a set of "printf"-like routines.  These
** routines format strings much like the printf() from the standard C
** library, though the implementation here has enhancements to support
** SQLite.
*/
//#include "sqliteInt.h"

/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define etRADIX     0 /* non-decimal integer types.  %x %o */
#define etFLOAT     1 /* Floating point.  %f */
#define etEXP       2 /* Exponentional notation. %e and %E */
#define etGENERIC   3 /* Floating or exponential, depending on exponent. %g */
#define etSIZE      4 /* Return number of characters processed so far. %n */
#define etSTRING    5 /* Strings. %s */
#define etDYNSTRING 6 /* Dynamically allocated strings. %z */
#define etPERCENT   7 /* Percent symbol. %% */
#define etCHARX     8 /* Characters. %c */
/* The rest are extensions, not normally found in printf() */
#define etSQLESCAPE 9 /* Strings with '\'' doubled.  %q */
#define etSQLESCAPE2                                                                                                   \
	10 /* Strings with '\'' doubled and enclosed in '',                                                                \
	     NULL pointers replaced by SQL NULL.  %Q */
#define etTOKEN      11 /* a pointer to a Token structure */
#define etSRCLIST    12 /* a pointer to a SrcList */
#define etPOINTER    13 /* The %p conversion */
#define etSQLESCAPE3 14 /* %w -> Strings with '\"' doubled */
#define etORDINAL    15 /* %r -> 1st, 2nd, 3rd, 4th, etc.  English only */
#define etDECIMAL    16 /* %d or %u, but not %x, %o */

#define etINVALID 17 /* Any unrecognized conversion type */

/*
** An "etByte" is an 8-bit unsigned value.
*/
typedef unsigned char etByte;

/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct et_info { /* Information about each format field */
	char fmttype;        /* The format field code letter */
	etByte base;         /* The base for radix conversion */
	etByte flags;        /* One or more of FLAG_ constants below */
	etByte type;         /* Conversion paradigm */
	etByte charset;      /* Offset into aDigits[] of the digits string */
	etByte prefix;       /* Offset into aPrefix[] of the prefix string */
} et_info;

/*
** Allowed values for et_info.flags
*/
#define FLAG_SIGNED 1 /* True if the value to convert is signed */
#define FLAG_STRING 4 /* Allow infinite precision */

/*
** The following table is searched linearly, so it is good to put the
** most frequently used conversion types first.
*/
static const char aDigits[] = "0123456789ABCDEF0123456789abcdef";
static const char aPrefix[] = "-x0\000X0";
static const et_info fmtinfo[] = {
    {'d', 10, 1, etDECIMAL, 0, 0},
    {'s', 0, 4, etSTRING, 0, 0},
    {'g', 0, 1, etGENERIC, 30, 0},
    {'z', 0, 4, etDYNSTRING, 0, 0},
    {'q', 0, 4, etSQLESCAPE, 0, 0},
    {'Q', 0, 4, etSQLESCAPE2, 0, 0},
    {'w', 0, 4, etSQLESCAPE3, 0, 0},
    {'c', 0, 0, etCHARX, 0, 0},
    {'o', 8, 0, etRADIX, 0, 2},
    {'u', 10, 0, etDECIMAL, 0, 0},
    {'x', 16, 0, etRADIX, 16, 1},
    {'X', 16, 0, etRADIX, 0, 4},
#ifndef SQLITE_OMIT_FLOATING_POINT
    {'f', 0, 1, etFLOAT, 0, 0},
    {'e', 0, 1, etEXP, 30, 0},
    {'E', 0, 1, etEXP, 14, 0},
    {'G', 0, 1, etGENERIC, 14, 0},
#endif
    {'i', 10, 1, etDECIMAL, 0, 0},
    {'n', 0, 0, etSIZE, 0, 0},
    {'%', 0, 0, etPERCENT, 0, 0},
    {'p', 16, 0, etPOINTER, 0, 1},

    /* All the rest are undocumented and are for internal use only */
    {'T', 0, 0, etTOKEN, 0, 0},
    {'S', 0, 0, etSRCLIST, 0, 0},
    {'r', 10, 1, etORDINAL, 0, 0},
};

/*
** If SQLITE_OMIT_FLOATING_POINT is defined, then none of the floating point
** conversions will work.
*/
#ifndef SQLITE_OMIT_FLOATING_POINT
/*
** "*val" is a double such that 0.1 <= *val < 10.0
** Return the ascii code for the leading digit of *val, then
** multiply "*val" by 10.0 to renormalize.
**
** Example:
**     input:     *val = 3.14159
**     output:    *val = 1.4159    function return = '3'
**
** The counter *cnt is incremented each time.  After counter exceeds
** 16 (the number of significant digits in a 64-bit float) '0' is
** always returned.
*/
static char et_getdigit(LONGDOUBLE_TYPE *val, int *cnt) {
	int digit;
	LONGDOUBLE_TYPE d;
	if ((*cnt) <= 0)
		return '0';
	(*cnt)--;
	digit = (int)*val;
	d = digit;
	digit += '0';
	*val = (*val - d) * 10.0;
	return (char)digit;
}
#endif /* SQLITE_OMIT_FLOATING_POINT */

/*
** Set the StrAccum object to an error mode.
*/
static void setStrAccumError(StrAccum *p, u8 eError) {
	assert(eError == STRACCUM_NOMEM || eError == STRACCUM_TOOBIG);
	p->accError = eError;
	p->nAlloc = 0;
}

/*
** On machines with a small stack size, you can redefine the
** SQLITE_PRINT_BUF_SIZE to be something smaller, if desired.
*/
#ifndef SQLITE_PRINT_BUF_SIZE
#define SQLITE_PRINT_BUF_SIZE 70
#endif
#define etBUFSIZE SQLITE_PRINT_BUF_SIZE /* Size of the output buffer */

/*
** Render a string given by "fmt" into the StrAccum object.
*/
void sqlite3VXPrintf(StrAccum *pAccum, /* Accumulate results here */
                     const char *fmt,  /* Format string */
                     va_list ap        /* arguments */
) {
	int c;                     /* Next character in the format string */
	char *bufpt;               /* Pointer to the conversion buffer */
	int precision;             /* Precision of the current field */
	int length;                /* Length of the field */
	int idx;                   /* A general purpose loop counter */
	int width;                 /* Width of the current field */
	etByte flag_leftjustify;   /* True if "-" flag is present */
	etByte flag_prefix;        /* '+' or ' ' or 0 for prefix */
	etByte flag_alternateform; /* True if "#" flag is present */
	etByte flag_altform2;      /* True if "!" flag is present */
	etByte flag_zeropad;       /* True if field width constant starts with zero */
	etByte flag_long;          /* 1 for the "l" flag, 2 for "ll", 0 by default */
	etByte done;               /* Loop termination flag */
	etByte cThousand;          /* Thousands separator for %d and %u */
	etByte xtype = etINVALID;  /* Conversion paradigm */
	u8 bArgList;               /* True for SQLITE_PRINTF_SQLFUNC */
	char prefix;               /* Prefix character.  "+" or "-" or " " or '\0'. */
	sqlite_uint64 longvalue;   /* Value for integer types */
	LONGDOUBLE_TYPE realvalue; /* Value for real types */
	const et_info *infop;      /* Pointer to the appropriate info structure */
	char *zOut;                /* Rendering buffer */
	int nOut;                  /* Size of the rendering buffer */
	char *zExtra = 0;          /* Malloced memory used by some conversion */
#ifndef SQLITE_OMIT_FLOATING_POINT
	int exp, e2;     /* exponent of real numbers */
	int nsd;         /* Number of significant digits returned */
	double rounder;  /* Used for rounding floating point values */
	etByte flag_dp;  /* True if decimal point should be shown */
	etByte flag_rtz; /* True if trailing zeros should be removed */
#endif
	//  void *pArgList = 0; /* Arguments for SQLITE_PRINTF_SQLFUNC */
	char buf[etBUFSIZE]; /* Conversion buffer */

	/* pAccum never starts out with an empty buffer that was obtained from
	** malloc().  This precondition is required by the mprintf("%z...")
	** optimization. */
	assert(pAccum->nChar > 0 || (pAccum->printfFlags & SQLITE_PRINTF_MALLOCED) == 0);

	bufpt = 0;
	if ((pAccum->printfFlags & SQLITE_PRINTF_SQLFUNC) != 0) {
		//    pArgList = va_arg(ap, PrintfArguments*);
		//    bArgList = 1;
		assert(0);
	} else {
		bArgList = 0;
	}
	for (; (c = (*fmt)) != 0; ++fmt) {
		if (c != '%') {
			bufpt = (char *)fmt;
#if HAVE_STRCHRNUL
			fmt = strchrnul(fmt, '%');
#else
			do {
				fmt++;
			} while (*fmt && *fmt != '%');
#endif
			sqlite3StrAccumAppend(pAccum, bufpt, (int)(fmt - bufpt));
			if (*fmt == 0)
				break;
		}
		if ((c = (*++fmt)) == 0) {
			sqlite3StrAccumAppend(pAccum, "%", 1);
			break;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_prefix = cThousand = flag_alternateform = flag_altform2 = flag_zeropad = 0;
		done = 0;
		do {
			switch (c) {
			case '-':
				flag_leftjustify = 1;
				break;
			case '+':
				flag_prefix = '+';
				break;
			case ' ':
				flag_prefix = ' ';
				break;
			case '#':
				flag_alternateform = 1;
				break;
			case '!':
				flag_altform2 = 1;
				break;
			case '0':
				flag_zeropad = 1;
				break;
			case ',':
				cThousand = ',';
				break;
			default:
				done = 1;
				break;
			}
		} while (!done && (c = (*++fmt)) != 0);
		/* Get the field width */
		if (c == '*') {
			if (bArgList) {
				assert(0);
				// width = (int)getIntArg(pArgList);
			} else {
				width = va_arg(ap, int);
			}
			if (width < 0) {
				flag_leftjustify = 1;
				width = width >= -2147483647 ? -width : 0;
			}
			c = *++fmt;
		} else {
			unsigned wx = 0;
			while (c >= '0' && c <= '9') {
				wx = wx * 10 + c - '0';
				c = *++fmt;
			}
			// testcase( wx>0x7fffffff );
			width = wx & 0x7fffffff;
		}
		assert(width >= 0);
#ifdef SQLITE_PRINTF_PRECISION_LIMIT
		if (width > SQLITE_PRINTF_PRECISION_LIMIT) {
			width = SQLITE_PRINTF_PRECISION_LIMIT;
		}
#endif

		/* Get the precision */
		if (c == '.') {
			c = *++fmt;
			if (c == '*') {
				if (bArgList) {
					assert(0);
					//     precision = (int)getIntArg(pArgList);
				} else {
					precision = va_arg(ap, int);
				}
				c = *++fmt;
				if (precision < 0) {
					precision = precision >= -2147483647 ? -precision : -1;
				}
			} else {
				unsigned px = 0;
				while (c >= '0' && c <= '9') {
					px = px * 10 + c - '0';
					c = *++fmt;
				}
				// testcase( px>0x7fffffff );
				precision = px & 0x7fffffff;
			}
		} else {
			precision = -1;
		}
		assert(precision >= (-1));
#ifdef SQLITE_PRINTF_PRECISION_LIMIT
		if (precision > SQLITE_PRINTF_PRECISION_LIMIT) {
			precision = SQLITE_PRINTF_PRECISION_LIMIT;
		}
#endif

		/* Get the conversion type modifier */
		if (c == 'l') {
			flag_long = 1;
			c = *++fmt;
			if (c == 'l') {
				flag_long = 2;
				c = *++fmt;
			}
		} else {
			flag_long = 0;
		}
		/* Fetch the info entry for the field */
		infop = &fmtinfo[0];
		xtype = etINVALID;
		for (idx = 0; idx < ArraySize(fmtinfo); idx++) {
			if (c == fmtinfo[idx].fmttype) {
				infop = &fmtinfo[idx];
				xtype = infop->type;
				break;
			}
		}

		/*
		** At this point, variables are initialized as follows:
		**
		**   flag_alternateform          TRUE if a '#' is present.
		**   flag_altform2               TRUE if a '!' is present.
		**   flag_prefix                 '+' or ' ' or zero
		**   flag_leftjustify            TRUE if a '-' is present or if the
		**                               field width was negative.
		**   flag_zeropad                TRUE if the width began with 0.
		**   flag_long                   1 for "l", 2 for "ll"
		**   width                       The specified field width.  This is
		**                               always non-negative.  Zero is the default.
		**   precision                   The specified precision.  The default
		**                               is -1.
		**   xtype                       The class of the conversion.
		**   infop                       Pointer to the appropriate info struct.
		*/
		switch (xtype) {
		case etPOINTER:
			flag_long = sizeof(char *) == sizeof(i64) ? 2 : sizeof(char *) == sizeof(long int) ? 1 : 0;
			/* Fall through into the next case */
		case etORDINAL:
		case etRADIX:
			cThousand = 0;
			/* Fall through into the next case */
		case etDECIMAL:
			if (infop->flags & FLAG_SIGNED) {
				i64 v;
				if (bArgList) {
					// v = getIntArg(pArgList);
					assert(0);
				} else if (flag_long) {
					if (flag_long == 2) {
						v = va_arg(ap, i64);
					} else {
						v = va_arg(ap, long int);
					}
				} else {
					v = va_arg(ap, int);
				}
				if (v < 0) {
					if (v == SMALLEST_INT64) {
						longvalue = ((u64)1) << 63;
					} else {
						longvalue = -v;
					}
					prefix = '-';
				} else {
					longvalue = v;
					prefix = flag_prefix;
				}
			} else {
				if (bArgList) {
					assert(0);
					//   longvalue = (u64)getIntArg(pArgList);
				} else if (flag_long) {
					if (flag_long == 2) {
						longvalue = va_arg(ap, u64);
					} else {
						longvalue = va_arg(ap, unsigned long int);
					}
				} else {
					longvalue = va_arg(ap, unsigned int);
				}
				prefix = 0;
			}
			if (longvalue == 0)
				flag_alternateform = 0;
			if (flag_zeropad && precision < width - (prefix != 0)) {
				precision = width - (prefix != 0);
			}
			if (precision < etBUFSIZE - 10 - etBUFSIZE / 3) {
				nOut = etBUFSIZE;
				zOut = buf;
			} else {
				u64 n = (u64)precision + 10 + precision / 3;
				zOut = zExtra = sqlite3Malloc(n);
				if (zOut == 0) {
					setStrAccumError(pAccum, STRACCUM_NOMEM);
					return;
				}
				nOut = (int)n;
			}
			bufpt = &zOut[nOut - 1];
			if (xtype == etORDINAL) {
				static const char zOrd[] = "thstndrd";
				int x = (int)(longvalue % 10);
				if (x >= 4 || (longvalue / 10) % 10 == 1) {
					x = 0;
				}
				*(--bufpt) = zOrd[x * 2 + 1];
				*(--bufpt) = zOrd[x * 2];
			}
			{
				const char *cset = &aDigits[infop->charset];
				u8 base = infop->base;
				do { /* Convert to ascii */
					*(--bufpt) = cset[longvalue % base];
					longvalue = longvalue / base;
				} while (longvalue > 0);
			}
			length = (int)(&zOut[nOut - 1] - bufpt);
			while (precision > length) {
				*(--bufpt) = '0'; /* Zero pad */
				length++;
			}
			if (cThousand) {
				int nn = (length - 1) / 3; /* Number of "," to insert */
				int ix = (length - 1) % 3 + 1;
				bufpt -= nn;
				for (idx = 0; nn > 0; idx++) {
					bufpt[idx] = bufpt[idx + nn];
					ix--;
					if (ix == 0) {
						bufpt[++idx] = cThousand;
						nn--;
						ix = 3;
					}
				}
			}
			if (prefix)
				*(--bufpt) = prefix;                   /* Add sign */
			if (flag_alternateform && infop->prefix) { /* Add "0" or "0x" */
				const char *pre;
				char x;
				pre = &aPrefix[infop->prefix];
				for (; (x = (*pre)) != 0; pre++)
					*(--bufpt) = x;
			}
			length = (int)(&zOut[nOut - 1] - bufpt);
			break;
		case etFLOAT:
		case etEXP:
		case etGENERIC:
			if (bArgList) {
				assert(0);
				//          realvalue = getDoubleArg(pArgList);
			} else {
				realvalue = va_arg(ap, double);
			}
#ifdef SQLITE_OMIT_FLOATING_POINT
			length = 0;
#else
			if (precision < 0)
				precision = 6; /* Set default precision */
			if (realvalue < 0.0) {
				realvalue = -realvalue;
				prefix = '-';
			} else {
				prefix = flag_prefix;
			}
			if (xtype == etGENERIC && precision > 0)
				precision--;
			// testcase( precision>0xfff );
			for (idx = precision & 0xfff, rounder = 0.5; idx > 0; idx--, rounder *= 0.1) {
			}
			if (xtype == etFLOAT)
				realvalue += rounder;
			/* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
			exp = 0;
			if (sqlite3IsNaN((double)realvalue)) {
				bufpt = "NaN";
				length = 3;
				break;
			}
			if (realvalue > 0.0) {
				LONGDOUBLE_TYPE scale = 1.0;
				while (realvalue >= 1e100 * scale && exp <= 350) {
					scale *= 1e100;
					exp += 100;
				}
				while (realvalue >= 1e10 * scale && exp <= 350) {
					scale *= 1e10;
					exp += 10;
				}
				while (realvalue >= 10.0 * scale && exp <= 350) {
					scale *= 10.0;
					exp++;
				}
				realvalue /= scale;
				while (realvalue < 1e-8) {
					realvalue *= 1e8;
					exp -= 8;
				}
				while (realvalue < 1.0) {
					realvalue *= 10.0;
					exp--;
				}
				if (exp > 350) {
					bufpt = buf;
					buf[0] = prefix;
					memcpy(buf + (prefix != 0), "Inf", 4);
					length = 3 + (prefix != 0);
					break;
				}
			}
			bufpt = buf;
			/*
			** If the field type is etGENERIC, then convert to either etEXP
			** or etFLOAT, as appropriate.
			*/
			if (xtype != etFLOAT) {
				realvalue += rounder;
				if (realvalue >= 10.0) {
					realvalue *= 0.1;
					exp++;
				}
			}
			if (xtype == etGENERIC) {
				flag_rtz = !flag_alternateform;
				if (exp < -4 || exp > precision) {
					xtype = etEXP;
				} else {
					precision = precision - exp;
					xtype = etFLOAT;
				}
			} else {
				flag_rtz = flag_altform2;
			}
			if (xtype == etEXP) {
				e2 = 0;
			} else {
				e2 = exp;
			}
			if (MAX(e2, 0) + (i64)precision + (i64)width > etBUFSIZE - 15) {
				bufpt = zExtra = sqlite3Malloc(MAX(e2, 0) + (i64)precision + (i64)width + 15);
				if (bufpt == 0) {
					setStrAccumError(pAccum, STRACCUM_NOMEM);
					return;
				}
			}
			zOut = bufpt;
			nsd = 16 + flag_altform2 * 10;
			flag_dp = (precision > 0 ? 1 : 0) | flag_alternateform | flag_altform2;
			/* The sign in front of the number */
			if (prefix) {
				*(bufpt++) = prefix;
			}
			/* Digits prior to the decimal point */
			if (e2 < 0) {
				*(bufpt++) = '0';
			} else {
				for (; e2 >= 0; e2--) {
					*(bufpt++) = et_getdigit(&realvalue, &nsd);
				}
			}
			/* The decimal point */
			if (flag_dp) {
				*(bufpt++) = '.';
			}
			/* "0" digits after the decimal point but before the first
			** significant digit of the number */
			for (e2++; e2 < 0; precision--, e2++) {
				assert(precision > 0);
				*(bufpt++) = '0';
			}
			/* Significant digits after the decimal point */
			while ((precision--) > 0) {
				*(bufpt++) = et_getdigit(&realvalue, &nsd);
			}
			/* Remove trailing zeros and the "." if no digits follow the "." */
			if (flag_rtz && flag_dp) {
				while (bufpt[-1] == '0')
					*(--bufpt) = 0;
				assert(bufpt > zOut);
				if (bufpt[-1] == '.') {
					if (flag_altform2) {
						*(bufpt++) = '0';
					} else {
						*(--bufpt) = 0;
					}
				}
			}
			/* Add the "eNNN" suffix */
			if (xtype == etEXP) {
				*(bufpt++) = aDigits[infop->charset];
				if (exp < 0) {
					*(bufpt++) = '-';
					exp = -exp;
				} else {
					*(bufpt++) = '+';
				}
				if (exp >= 100) {
					*(bufpt++) = (char)((exp / 100) + '0'); /* 100's digit */
					exp %= 100;
				}
				*(bufpt++) = (char)(exp / 10 + '0'); /* 10's digit */
				*(bufpt++) = (char)(exp % 10 + '0'); /* 1's digit */
			}
			*bufpt = 0;

			/* The converted number is in buf[] and zero terminated. Output it.
			** Note that the number is in the usual order, not reversed as with
			** integer conversions. */
			length = (int)(bufpt - zOut);
			bufpt = zOut;

			/* Special case:  Add leading zeros if the flag_zeropad flag is
			** set and we are not left justified */
			if (flag_zeropad && !flag_leftjustify && length < width) {
				int i;
				int nPad = width - length;
				for (i = width; i >= nPad; i--) {
					bufpt[i] = bufpt[i - nPad];
				}
				i = prefix != 0;
				while (nPad--)
					bufpt[i++] = '0';
				length = width;
			}
#endif /* !defined(SQLITE_OMIT_FLOATING_POINT) */
			break;
		case etSIZE:
			if (!bArgList) {
				*(va_arg(ap, int *)) = pAccum->nChar;
			}
			length = width = 0;
			break;
		case etPERCENT:
			buf[0] = '%';
			bufpt = buf;
			length = 1;
			break;
		case etCHARX:
			if (bArgList) {
				assert(0);
				/*
			  bufpt = getTextArg(pArgList);
			  length = 1;
			  if( bufpt ){
				buf[0] = c = *(bufpt++);
				if( (c&0xc0)==0xc0 ){
				  while( length<4 && (bufpt[0]&0xc0)==0x80 ){
				    buf[length++] = *(bufpt++);
				  }
				}
			  }else{
				buf[0] = 0;
			  } */
			} else {
				unsigned int ch = va_arg(ap, unsigned int);
				if (ch < 0x00080) {
					buf[0] = ch & 0xff;
					length = 1;
				} else if (ch < 0x00800) {
					buf[0] = 0xc0 + (u8)((ch >> 6) & 0x1f);
					buf[1] = 0x80 + (u8)(ch & 0x3f);
					length = 2;
				} else if (ch < 0x10000) {
					buf[0] = 0xe0 + (u8)((ch >> 12) & 0x0f);
					buf[1] = 0x80 + (u8)((ch >> 6) & 0x3f);
					buf[2] = 0x80 + (u8)(ch & 0x3f);
					length = 3;
				} else {
					buf[0] = 0xf0 + (u8)((ch >> 18) & 0x07);
					buf[1] = 0x80 + (u8)((ch >> 12) & 0x3f);
					buf[2] = 0x80 + (u8)((ch >> 6) & 0x3f);
					buf[3] = 0x80 + (u8)(ch & 0x3f);
					length = 4;
				}
			}
			if (precision > 1) {
				width -= precision - 1;
				if (width > 1 && !flag_leftjustify) {
					sqlite3AppendChar(pAccum, width - 1, ' ');
					width = 0;
				}
				while (precision-- > 1) {
					sqlite3StrAccumAppend(pAccum, buf, length);
				}
			}
			bufpt = buf;
			flag_altform2 = 1;
			goto adjust_width_for_utf8;
		case etSTRING:
		case etDYNSTRING:
			if (bArgList) {
				assert(0);
				//          bufpt = getTextArg(pArgList);
				//          xtype = etSTRING;
			} else {
				bufpt = va_arg(ap, char *);
			}
			if (bufpt == 0) {
				bufpt = "";
			} else if (xtype == etDYNSTRING) {
				//
				//          if( pAccum->nChar==0 && pAccum->mxAlloc && width==0 && precision<0 ){
				//            /* Special optimization for sqlite3_mprintf("%z..."):
				//            ** Extend an existing memory allocation rather than creating
				//            ** a new one. */
				//            assert( (pAccum->printfFlags&SQLITE_PRINTF_MALLOCED)==0 );
				//            pAccum->zText = bufpt;
				//            pAccum->nAlloc = sqlite3DbMallocSize(pAccum->db, bufpt);
				//            pAccum->nChar = 0x7fffffff & (int)strlen(bufpt);
				//            pAccum->printfFlags |= SQLITE_PRINTF_MALLOCED;
				//            length = 0;
				//            break;
				//          }
				zExtra = bufpt;
			}
			if (precision >= 0) {
				if (flag_altform2) {
					/* Set length to the number of bytes needed in order to display
					** precision characters */
					unsigned char *z = (unsigned char *)bufpt;
					while (precision-- > 0 && z[0]) {
						SQLITE_SKIP_UTF8(z);
					}
					length = (int)(z - (unsigned char *)bufpt);
				} else {
					for (length = 0; length < precision && bufpt[length]; length++) {
					}
				}
			} else {
				length = 0x7fffffff & (int)strlen(bufpt);
			}
		adjust_width_for_utf8:
			if (flag_altform2 && width > 0) {
				/* Adjust width to account for extra bytes in UTF-8 characters */
				int ii = length - 1;
				while (ii >= 0)
					if ((bufpt[ii--] & 0xc0) == 0x80)
						width++;
			}
			break;
		case etSQLESCAPE:    /* %q: Escape ' characters */
		case etSQLESCAPE2:   /* %Q: Escape ' and enclose in '...' */
		case etSQLESCAPE3: { /* %w: Escape " characters */
			int i, j, k, n, isnull;
			int needQuote;
			char ch;
			char q = ((xtype == etSQLESCAPE3) ? '"' : '\''); /* Quote character */
			char *escarg;

			if (bArgList) {
				assert(0);
				// escarg = getTextArg(pArgList);
			} else {
				escarg = va_arg(ap, char *);
			}
			isnull = escarg == 0;
			if (isnull)
				escarg = (xtype == etSQLESCAPE2 ? "NULL" : "(NULL)");
			/* For %q, %Q, and %w, the precision is the number of byte (or
			** characters if the ! flags is present) to use from the input.
			** Because of the extra quoting characters inserted, the number
			** of output characters may be larger than the precision.
			*/
			k = precision;
			for (i = n = 0; k != 0 && (ch = escarg[i]) != 0; i++, k--) {
				if (ch == q)
					n++;
				if (flag_altform2 && (ch & 0xc0) == 0xc0) {
					while ((escarg[i + 1] & 0xc0) == 0x80) {
						i++;
					}
				}
			}
			needQuote = !isnull && xtype == etSQLESCAPE2;
			n += i + 3;
			if (n > etBUFSIZE) {
				bufpt = zExtra = sqlite3Malloc(n);
				if (bufpt == 0) {
					setStrAccumError(pAccum, STRACCUM_NOMEM);
					return;
				}
			} else {
				bufpt = buf;
			}
			j = 0;
			if (needQuote)
				bufpt[j++] = q;
			k = i;
			for (i = 0; i < k; i++) {
				bufpt[j++] = ch = escarg[i];
				if (ch == q)
					bufpt[j++] = ch;
			}
			if (needQuote)
				bufpt[j++] = q;
			bufpt[j] = 0;
			length = j;
			goto adjust_width_for_utf8;
		}
		case etTOKEN: {
			/*
		  Token *pToken;
		  if( (pAccum->printfFlags & SQLITE_PRINTF_INTERNAL)==0 ) return;
		  pToken = va_arg(ap, Token*);
		  assert( bArgList==0 );
		  if( pToken && pToken->n ){
			sqlite3StrAccumAppend(pAccum, (const char*)pToken->z, pToken->n);
		  }
		  length = width = 0;
		  */
			assert(0);
			break;
		}
		case etSRCLIST: {
			/*
			   SrcList *pSrc;
			 int k;
			 struct SrcList_item *pItem;
			 if( (pAccum->printfFlags & SQLITE_PRINTF_INTERNAL)==0 ) return;
			 pSrc = va_arg(ap, SrcList*);
			 k = va_arg(ap, int);
			 pItem = &pSrc->a[k];
			 assert( bArgList==0 );
			 assert( k>=0 && k<pSrc->nSrc );
			 if( pItem->zDatabase ){
			   sqlite3StrAccumAppendAll(pAccum, pItem->zDatabase);
			   sqlite3StrAccumAppend(pAccum, ".", 1);
			 }
			 sqlite3StrAccumAppendAll(pAccum, pItem->zName);
			 length = width = 0;
			 */
			assert(0);
			break;
		}
		default: {
			assert(xtype == etINVALID);
			return;
		}
		} /* End switch over the format type */
		/*
		** The text of the conversion is pointed to by "bufpt" and is
		** "length" characters long.  The field width is "width".  Do
		** the output.  Both length and width are in bytes, not characters,
		** at this point.  If the "!" flag was present on string conversions
		** indicating that width and precision should be expressed in characters,
		** then the values have been translated prior to reaching this point.
		*/
		width -= length;
		if (width > 0) {
			if (!flag_leftjustify)
				sqlite3AppendChar(pAccum, width, ' ');
			sqlite3StrAccumAppend(pAccum, bufpt, length);
			if (flag_leftjustify)
				sqlite3AppendChar(pAccum, width, ' ');
		} else {
			sqlite3StrAccumAppend(pAccum, bufpt, length);
		}

		if (zExtra) {
			free(zExtra);
			zExtra = 0;
		}
	} /* End for loop over the format string */
} /* End of function */

/*
** Enlarge the memory allocation on a StrAccum object so that it is
** able to accept at least N more bytes of text.
**
** Return the number of bytes of text that StrAccum is able to accept
** after the attempted enlargement.  The value returned might be zero.
*/
static int sqlite3StrAccumEnlarge(StrAccum *p, int N) {
	char *zNew;
	assert(p->nChar + (i64)N >= p->nAlloc); /* Only called if really needed */
	if (p->accError) {
		// testcase(p->accError==STRACCUM_TOOBIG);
		// testcase(p->accError==STRACCUM_NOMEM);
		return 0;
	}
	if (p->mxAlloc == 0) {
		N = p->nAlloc - p->nChar - 1;
		setStrAccumError(p, STRACCUM_TOOBIG);
		return N;
	} else {
		char *zOld = isMalloced(p) ? p->zText : 0;
		i64 szNew = p->nChar;
		szNew += N + 1;
		if (szNew + p->nChar <= p->mxAlloc) {
			/* Force exponential buffer size growth as long as it does not overflow,
			** to avoid having to call this routine too often */
			szNew += p->nChar;
		}
		if (szNew > p->mxAlloc) {
			sqlite3StrAccumReset(p);
			setStrAccumError(p, STRACCUM_TOOBIG);
			return 0;
		} else {
			p->nAlloc = (int)szNew;
		}
		if (p->db) {
			assert(0);
			//	zNew = sqlite3DbRealloc(p->db, zOld, p->nAlloc);
		} else {
			zNew = sqlite3_realloc64(zOld, p->nAlloc);
		}
		if (zNew) {
			assert(p->zText != 0 || p->nChar == 0);
			if (!isMalloced(p) && p->nChar > 0)
				memcpy(zNew, p->zText, p->nChar);
			p->zText = zNew;
			// p->nAlloc = sqlite3DbMallocSize(p->db, zNew);
			p->printfFlags |= SQLITE_PRINTF_MALLOCED;
		} else {
			sqlite3StrAccumReset(p);
			setStrAccumError(p, STRACCUM_NOMEM);
			return 0;
		}
	}
	return N;
}

/*
** Append N copies of character c to the given string buffer.
*/
void sqlite3AppendChar(StrAccum *p, int N, char c) {
	// testcase( p->nChar + (i64)N > 0x7fffffff );
	if (p->nChar + (i64)N >= p->nAlloc && (N = sqlite3StrAccumEnlarge(p, N)) <= 0) {
		return;
	}
	while ((N--) > 0)
		p->zText[p->nChar++] = c;
}

/*
** The StrAccum "p" is not large enough to accept N new bytes of z[].
** So enlarge if first, then do the append.
**
** This is a helper routine to sqlite3StrAccumAppend() that does special-case
** work (enlarging the buffer) using tail recursion, so that the
** sqlite3StrAccumAppend() routine can use fast calling semantics.
*/
static void enlargeAndAppend(StrAccum *p, const char *z, int N) {
	N = sqlite3StrAccumEnlarge(p, N);
	if (N > 0) {
		memcpy(&p->zText[p->nChar], z, N);
		p->nChar += N;
	}
}

/*
** Append N bytes of text from z to the StrAccum object.  Increase the
** size of the memory allocation for StrAccum if necessary.
*/
void sqlite3StrAccumAppend(StrAccum *p, const char *z, int N) {
	assert(z != 0 || N == 0);
	assert(p->zText != 0 || p->nChar == 0 || p->accError);
	assert(N >= 0);
	assert(p->accError == 0 || p->nAlloc == 0);
	if (p->nChar + N >= p->nAlloc) {
		enlargeAndAppend(p, z, N);
	} else if (N) {
		assert(p->zText);
		p->nChar += N;
		memcpy(&p->zText[p->nChar - N], z, N);
	}
}

/*
** Append the complete text of zero-terminated string z[] to the p string.
*/
void sqlite3StrAccumAppendAll(StrAccum *p, const char *z) {
	sqlite3StrAccumAppend(p, z, strlen(z));
}

/*
** Finish off a string by making sure it is zero-terminated.
** Return a pointer to the resulting string.  Return a NULL
** pointer if any kind of error was encountered.
*/
static char *strAccumFinishRealloc(StrAccum *p) {
	char *zText;
	assert(p->mxAlloc > 0 && !isMalloced(p));
	zText = malloc(p->nChar + 1);
	if (zText) {
		memcpy(zText, p->zText, p->nChar + 1);
		p->printfFlags |= SQLITE_PRINTF_MALLOCED;
	} else {
		setStrAccumError(p, STRACCUM_NOMEM);
	}
	p->zText = zText;
	return zText;
}
char *sqlite3StrAccumFinish(StrAccum *p) {
	if (p->zText) {
		p->zText[p->nChar] = 0;
		if (p->mxAlloc > 0 && !isMalloced(p)) {
			return strAccumFinishRealloc(p);
		}
	}
	return p->zText;
}

/*
** Reset an StrAccum string.  Reclaim all malloced memory.
*/
void sqlite3StrAccumReset(StrAccum *p) {
	if (isMalloced(p)) {
		// sqlite3DbFree(p->db, p->zText);
		free(p->zText);
		p->printfFlags &= ~SQLITE_PRINTF_MALLOCED;
	}
	p->zText = 0;
}

/*
** Initialize a string accumulator.
**
** p:     The accumulator to be initialized.
** db:    Pointer to a database connection.  May be NULL.  Lookaside
**        memory is used if not NULL. db->mallocFailed is set appropriately
**        when not NULL.
** zBase: An initial buffer.  May be NULL in which case the initial buffer
**        is malloced.
** n:     Size of zBase in bytes.  If total space requirements never exceed
**        n then no memory allocations ever occur.
** mx:    Maximum number of bytes to accumulate.  If mx==0 then no memory
**        allocations will ever occur.
*/
void sqlite3StrAccumInit(StrAccum *p, void *db, char *zBase, int n, int mx) {
	p->zText = zBase;
	assert(!db);
	p->db = db;
	p->nAlloc = n;
	p->mxAlloc = mx;
	p->nChar = 0;
	p->accError = 0;
	p->printfFlags = 0;
}

/*
** Print into memory obtained from sqlite3_malloc().  Omit the internal
** %-conversion extensions.
*/
char *sqlite3_vmprintf(const char *zFormat, va_list ap) {
	char *z;
	char zBase[SQLITE_PRINT_BUF_SIZE];
	StrAccum acc;

	sqlite3StrAccumInit(&acc, 0, zBase, sizeof(zBase), SQLITE_MAX_LENGTH);
	sqlite3VXPrintf(&acc, zFormat, ap);
	z = sqlite3StrAccumFinish(&acc);
	return z;
}

/*
** Print into memory obtained from sqlite3_malloc()().  Omit the internal
** %-conversion extensions.
*/
char *sqlite3_mprintf(const char *zFormat, ...) {
	va_list ap;
	char *z;
	va_start(ap, zFormat);
	z = sqlite3_vmprintf(zFormat, ap);
	va_end(ap);
	return z;
}

/*
** sqlite3_snprintf() works like snprintf() except that it ignores the
** current locale settings.  This is important for SQLite because we
** are not able to use a "," as the decimal point in place of "." as
** specified by some locales.
**
** Oops:  The first two arguments of sqlite3_snprintf() are backwards
** from the snprintf() standard.  Unfortunately, it is too late to change
** this without breaking compatibility, so we just have to live with the
** mistake.
**
** sqlite3_vsnprintf() is the varargs version.
*/
char *sqlite3_vsnprintf(int n, char *zBuf, const char *zFormat, va_list ap) {
	StrAccum acc;
	if (n <= 0)
		return zBuf;
#ifdef SQLITE_ENABLE_API_ARMOR
	if (zBuf == 0 || zFormat == 0) {
		(void)SQLITE_MISUSE_BKPT;
		if (zBuf)
			zBuf[0] = 0;
		return zBuf;
	}
#endif
	sqlite3StrAccumInit(&acc, 0, zBuf, n, 0);
	sqlite3VXPrintf(&acc, zFormat, ap);
	zBuf[acc.nChar] = 0;
	return zBuf;
}
char *sqlite3_snprintf(int n, char *zBuf, const char *zFormat, ...) {
	char *z;
	va_list ap;
	va_start(ap, zFormat);
	z = sqlite3_vsnprintf(n, zBuf, zFormat, ap);
	va_end(ap);
	return z;
}
