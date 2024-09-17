#ifndef _INCLUDE_MESSAGE_H_
#define _INCLUDE_MESSAGE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FTX_PAYLOAD_LENGTH_BYTES 10 ///< number of bytes to hold 77 bits of FTx payload data
#define FTX_NFIELDS 4
#define FTX_FIELD_SZ 32
#define FTX_MAX_MESSAGE_LENGTH  (FTX_NFIELDS*FTX_FIELD_SZ + (FTX_NFIELDS-1)*1 + SPACE_FOR_NULL)

/// Structure that holds the decoded message
typedef struct
{
    uint8_t payload[FTX_PAYLOAD_LENGTH_BYTES];
    uint16_t hash; ///< Hash value to be used in hash table and quick checking for duplicates
} ftx_message_t;

// i3: bits 74..76
#define FTX_MSG_I3(msg) (((msg)->payload[9] >> 3) & 0x07u)

// n3: bits 71..73
#define FTX_MSG_N3(msg) ( (((msg)->payload[8] << 2) & 0x04u) | (((msg)->payload[9] >> 6) & 0x03u) )

//
// ----------------------------------------------------------------------------------
// i3.n3 Example message                    Bits                 Total  Purpose
// ----------------------------------------------------------------------------------
// 0.0   FREE TEXT MSG                      f71                     71  Free text
// 0.1   K1ABC RR73; W9XYZ <KH1/KH7Z> -12   c28 c28 h10 r5          71  DXpedition Mode
// ft8_lib only, marked "tbd" in wsjtx/77bit.txt
// 0.2   PA3XYZ/P R 590003 IO91NP           c28 p1 R1 r3 s12 g25    70  EU VHF contest (ft8_lib)
// 0.3   WA9XYZ KA1ABC R 16A EMA            c28 c28 R1 n4 k3 S7     71  ARRL Field Day
// 0.4   WA9XYZ KA1ABC R 32A EMA            c28 c28 R1 n4 k3 S7     71  ARRL Field Day
// 0.5   123456789ABCDEF012                 t71                     71  Telemetry (18 hex)
// 0.6   K1ABC RR73; CQ W9XYZ EN37          c28 c28 g15             71  Contesting
// 0.7   ... tbd
// 1     WA9XYZ/R KA1ABC/R R FN42           c28 r1 c28 r1 R1 g15    74  Standard msg
// 2     PA3XYZ/P GM4ABC/P R JO22           c28 p1 c28 p1 R1 g15    74  EU VHF contest
// 3     TU; W9XYZ K1ABC R 579 MA           t1 c28 c28 R1 r3 s13    74  ARRL RTTY Roundup
// 4     <WA9XYZ> PJ4/KA1ABC RR73           h12 c58 h1 r2 c1        74  Nonstandard calls
// disagreement here:
// 5     TU; W9XYZ K1ABC R-07 FN            t1 c28 c28 R1 r7 g9     74  WWDIGI contest (ft8_lib)
// 5     <G4ABC/P> <PA9XYZ> R 570007 JO22DB h12 h22 R1 r3 s11 g25   74  EU VHF Contest (wsjtx/77bit.txt)
// 6     ... tbd
// 7     ... tbd

// Bits Information conveyed
// c1   First callsign is CQ; h12 is ignored
// c28  Standard callsign, CQ, DE, QRZ, or 22-bit hash
// c58  Nonstandard callsign, up to 11 characters
// f71  Free text, up to 13 characters
// g15  4-character grid, Report, RRR, RR73, 73, or blank
// g25  6-character grid
// g9   2-character grid field (WWDIGI): A-R(18) x A-R(18) = 324
// h1   Hashed callsign is the second callsign
// h10  Hashed callsign, 10 bits
// h22  Hashed callsign, 22 bits
// k3   Field Day Class: A, B, ... F
// n4   Number of transmitters: 1-16, 17-32
// p1   Callsign suffix /P
// r1   Callsign suffix /R
// r2   RRR, RR73, 73, or blank
// r3   Report: 2-9, displayed as 529 - 599 or 52 - 59
// r5   Report: -30 to +32, even numbers only
// r7   Report (WWDIGI): ?
// R1   R
// s11  Serial number (0-2047)
// s12  Serial number (0-4095)
// s13  Serial Number (0-7999) or State/Province
// S7   ARRL/RAC Section
// †1   TU;
// †71  Telemetry data, up to 18 hexadecimal digits


typedef enum
{
    FTX_MESSAGE_TYPE_FREE_TEXT,   // 0.0
    FTX_MESSAGE_TYPE_DXPEDITION,  // 0.1
    FTX_MESSAGE_TYPE_EU_VHF,      // 0.2
    FTX_MESSAGE_TYPE_ARRL_FD,     // 0.3
                                  // 0.4    also FTX_MESSAGE_TYPE_ARRL_FD
    FTX_MESSAGE_TYPE_TELEMETRY,   // 0.5
    FTX_MESSAGE_TYPE_CONTESTING,  // 0.6
    FTX_MESSAGE_TYPE_STANDARD,    // 1      using /R
                                  // 2      also FTX_MESSAGE_TYPE_STANDARD, using /P
    FTX_MESSAGE_TYPE_ARRL_RTTY,   // 3
    FTX_MESSAGE_TYPE_NONSTD_CALL, // 4
    FTX_MESSAGE_TYPE_WWDIGI,      // 5
                                  // 6 tbd
                                  // 7 tbd
    FTX_MESSAGE_TYPE_UNKNOWN      // Unknown or invalid type
} ftx_message_type_t;

typedef enum
{
    FTX_CALLSIGN_HASH_22_BITS,
    FTX_CALLSIGN_HASH_12_BITS,
    FTX_CALLSIGN_HASH_10_BITS
} ftx_callsign_hash_type_t;

typedef struct
{
    /// Called when a callsign is looked up by its 22/12/10 bit hash code
    bool (*lookup_hash)(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign);
    /// Called when a callsign should hashed and stored (by its 22, 12 and 10 bit hash codes)
    void (*save_hash)(const char* callsign, uint32_t n22, int *hash_idx);
} ftx_callsign_hash_interface_t;

typedef enum
{
    FTX_MESSAGE_RC_OK,
    FTX_MESSAGE_RC_PSKR_OK,
    FTX_MESSAGE_RC_ERROR_CALLSIGN1,
    FTX_MESSAGE_RC_ERROR_CALLSIGN2,
    FTX_MESSAGE_RC_ERROR_SUFFIX,
    FTX_MESSAGE_RC_ERROR_GRID,
    FTX_MESSAGE_RC_ERROR_TYPE
} ftx_message_rc_t;

// Callsign types and sizes:
// * Std. call (basecall) - 1-2 letter/digit prefix (at least one letter), 1 digit area code, 1-3 letter suffix,
//                          total 3-6 chars (exception: 7 character calls with prefixes 3DA0- and 3XA..3XZ-)
// * Ext. std. call - basecall followed by /R or /P
// * Nonstd. call - all the rest, limited to 3-11 characters either alphanumeric or stroke (/)
// In case a call is looked up from its hash value, the call is enclosed in angular brackets (<CA0LL>).

void ftx_message_init(ftx_message_t* msg);

ftx_message_type_t ftx_message_get_type(const ftx_message_t* msg);

// bool ftx_message_check_recipient(const ftx_message_t* msg, const char* callsign);

/// Pack (encode) a text message
ftx_message_rc_t ftx_message_encode(ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, const char* message_text);

/// Pack Type 1 (Standard 77-bit message) or Type 2 (ditto, with a "/P" call) message
/// Rules of callsign validity:
/// - call_to can be 'DE', 'CQ', 'QRZ', 'CQ_nnn' (three digits), or 'CQ_abcd' (four letters)
/// - nonstandard calls within <> brackets are allowed, if they don't contain '/'
ftx_message_rc_t ftx_message_encode_std(ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, const char* call_to, const char* call_de, const char* extra);

/// Pack Type 4 (One nonstandard call and one hashed call) message
ftx_message_rc_t ftx_message_encode_nonstd(ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, const char* call_to, const char* call_de, const char* extra);

void ftx_message_encode_free(const char* text);
void ftx_message_encode_telemetry_hex(const char* telemetry_hex);
void ftx_message_encode_telemetry(const uint8_t* telemetry);

ftx_message_rc_t ftx_message_decode(const ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, char* message, int *hash_idx);
ftx_message_rc_t ftx_message_decode_std(const ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, char* call_to, char* call_de, char* grid, int *hash_idx);
ftx_message_rc_t ftx_message_decode_nonstd(const ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, char* call_to, char* call_de, char* extra);
ftx_message_rc_t ftx_message_decode_DXpedition(const ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, char* call_rr, char* call_to, char* call_de, char* report, int *hash_idx);
ftx_message_rc_t ftx_message_decode_contesting(const ftx_message_t* msg, ftx_callsign_hash_interface_t* hash_if, char* call_to, char* call_de, char* grid, int *hash_idx);

void ftx_message_decode_free(const ftx_message_t* msg, char* text);
void ftx_message_decode_telemetry_hex(const ftx_message_t* msg, char* telemetry_hex);
void ftx_message_decode_telemetry(const ftx_message_t* msg, uint8_t* telemetry);

#ifdef FTX_DEBUG_PRINT
void ftx_message_print(ftx_message_t* msg);
#endif

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_MESSAGE_H_
