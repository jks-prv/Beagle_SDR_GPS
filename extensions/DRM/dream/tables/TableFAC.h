/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	Tables for FAC
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#if !defined(TABLE_FAC_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_)
#define TABLE_FAC_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_

#include <string>
#include "../GlobalDefinitions.h"

/* Definitions ****************************************************************/
/* ETSI ES201980V2.1.1: page 115, 7.5.3: ...FAC shall use 4-QAM mapping. A
   fixed code rate shall be applied...R_all=0.6...
   6 tailbits are used for the encoder to get in zero state ->
   65 [number of cells] * 2 [4-QAM] * 0.6 [code-rate] - 6 [tailbits] = 72 */
#define NUM_FAC_BITS_PER_BLOCK			72

/* iTableNumOfServices[a][b]
   a: Number of audio services
   b: Number of data services
   (6.3.4) */
extern const int iTableNumOfServices[5][5];

/* Language code */
#define LEN_TABLE_LANGUAGE_CODE			16

extern const std::string strTableLanguageCode[LEN_TABLE_LANGUAGE_CODE];

/* Programme Type codes */
#define LEN_TABLE_PROG_TYPE_CODE_TOT	32
#define LEN_TABLE_PROG_TYPE_CODE		30

extern const std::string strTableProgTypCod[LEN_TABLE_PROG_TYPE_CODE_TOT];

/* Country code table according to ISO 3166 */

#define LEN_TABLE_COUNTRY_CODE			244

#define LEN_COUNTRY_CODE				2
#define MAX_LEN_DESC_COUNTRY_CODE		44

struct elCountry {
    char	strcode [LEN_COUNTRY_CODE+1];
    char	strDesc [MAX_LEN_DESC_COUNTRY_CODE+1];
};

extern const struct elCountry TableCountryCode[LEN_TABLE_COUNTRY_CODE];

/* Get country name from ISO 3166 A2 */

std::string GetISOCountryName(const std::string strA2);

/* Language code table according to ISO/IEC 639-2 */

#define LEN_TABLE_ISO_LANGUAGE_CODE			505

#define LEN_ISO_LANGUAGE_CODE				3
#define MAX_LEN_DESC_ISO_LANGUAGE_CODE		44

struct elLanguage {
    char	strISOCode [LEN_ISO_LANGUAGE_CODE+1];
    char	strDesc [MAX_LEN_DESC_ISO_LANGUAGE_CODE+1];
};

extern const struct elLanguage TableISOLanguageCode[LEN_TABLE_ISO_LANGUAGE_CODE];

/* Get language name from ISO 3166 */

std::string GetISOLanguageName(const std::string strA3);

/* CIRAF zones */
#define LEN_TABLE_CIRAF_ZONES			86

extern const std::string strTableCIRAFzones[LEN_TABLE_CIRAF_ZONES];

#endif // !defined(TABLE_FAC_H__3B0_CA63_4344_BGDEB2B_23E7912__INCLUDED_)
