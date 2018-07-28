// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifdef EXT_S4285

#include "kiwi.h"
#undef B

#include <string>

#ifndef _FCOMPLEX_
#define _FCOMPLEX_

#define cmultReal(x,y)     ((x.re*y.re)-(x.im*y.im))
#define cmultImag(x,y)     ((x.re*y.im)+(x.im*y.re))

#define cmultRealConj(x,y) ((x.re*y.re)+(x.im*y.im))
#define cmultImagConj(x,y) ((x.im*y.re)-(x.re*y.im))

#endif

// data types
typedef struct { float re,im; } FComplex;

typedef struct {
	float data;
	FComplex rx_symbol;
	FComplex dx_symbol;
	FComplex error;
} FDemodulate;

// external API
typedef enum {
	B75N, B75S, B75L,
	B150N, B150S, B150L,
	B300N, B300S, B300L,
	B600N, B600S, B600L, B600U,
	B1200N, B1200S, B1200L, B1200U,
	B1800U,
	B2400N, B2400S, B2400L, B2400U,
	B3600U
} Kmode;

enum { TYPE_BITSTREAM_DATA = 0, TYPE_IQ_F32_DATA = 1, TYPE_REAL_S16_DATA = 2 };

typedef float doppler_t;

class CSt4285 {

public:
	enum { PREAMBLE_LENGTH = 80 };
	enum { TX_FILTER_LENGTH = 44 };
	enum { RX_FILTER_SIZE = 43 };
	enum { INTERLEAVER_NR_ROWS = 32 };

private:
	typedef struct{
		int bit1;
		int bit2;
	} ParityLookup;
	
	typedef	enum { DUPLEX_MODE, SIMPLEX_MODE } DuplexMode;
	typedef	enum { VOX_PTT, RTS_PTT,DTR_PTT } PttType;
	typedef enum { RX_HUNTING, RX_DATA } RxState;
	enum { FF_EQ_LENGTH = 30, FB_EQ_LENGTH = 14 };
	enum { KN_44 = FF_EQ_LENGTH + FB_EQ_LENGTH };
	enum { IO_QUEUE_LENGTH = 64000 };

	// Convolutional defines	
	enum { C10MASK = 0x0001 };
	enum { C11MASK = 0x0002 };
	enum { C13MASK = 0x0008 }; 
	enum { C14MASK = 0x0010 };
	enum { C16MASK = 0x0040 };
	enum { C20MASK = 0x0001 };
	enum { C23MASK = 0x0008 };
	enum { C24MASK = 0x0010 };
	enum { C25MASK = 0x0020 };
	enum { C26MASK = 0x0040 };

	enum { RX_75_BPS = 0x0000 };
	enum { RX_150_BPS = 0x0010 };
	enum { RX_300_BPS = 0x0020 };
	enum { RX_600_BPS = 0x0030 };
	enum { RX_1200_BPS = 0x0040 };
	enum { RX_2400_BPS = 0x0050 };
	enum { RX_1200U_BPS = 0x0060 };
	enum { RX_2400U_BPS = 0x0070 };
	enum { RX_3600U_BPS = 0x0080 };
	enum { TX_75_BPS = 0x0000 };
	enum { TX_150_BPS = 0x0001 };
	enum { TX_300_BPS = 0x0002 };
	enum { TX_600_BPS = 0x0003 };
	enum { TX_1200_BPS = 0x0004 };
	enum { TX_2400_BPS = 0x0005 };
	enum { TX_1200U_BPS = 0x0006 };
	enum { TX_2400U_BPS = 0x0007 };
	enum { TX_3600U_BPS = 0x0008 };

	enum { OUTPUT_DATA_LENGTH = 1000 };
	enum { NR_CONVOLUTIONAL_STATES = 64 };
	enum { NORMAL_PATH_LENGTH = 105 };
	enum { ALTERNATE_1_PATH_LENGTH = (NORMAL_PATH_LENGTH-4)};
	enum { ALTERNATE_2_PATH_LENGTH = (NORMAL_PATH_LENGTH-6)};

	// Receive defines 
	enum { LOW_RX_CHAN = 0 };
	enum { MID_RX_CHAN = 1 };
	enum { HIH_RX_CHAN = 2 };
	enum { RX_CHANNELS = 3 };
	
	// Design parameters
	enum { SYMBOL_RATE = 2400 };
	enum { DEFAULT_SAMPLE_RATE = 9600 };
	enum { SAMPLES_PER_SYMBOL = (DEFAULT_SAMPLE_RATE / SYMBOL_RATE) };		// i.e. 4
	enum { INPUT_DECIMATION = 2 };
	enum { SPS = (SAMPLES_PER_SYMBOL / INPUT_DECIMATION) };		// post-decimation samples-per-symbol, i.e. 2
	enum { CENTER_FREQUENCY = 1800 };
	enum { SYMBOLS_PER_FRAME = 256 };
	enum { PROBE_LENGTH = 16 };
	enum { DATA_LENGTH =  32 };
	enum { PPN_31 = 31 };		// preamble PN-sequence length
	enum { PPN_62 = ( PPN_31*2 ) };

	// Demodulator defines 
	enum { MSE_HISTORY_LENGTH = 256 };
	enum { SAMPLE_BLOCK_SIZE = (SYMBOLS_PER_FRAME * SAMPLES_PER_SYMBOL) };
	enum { HALF_SAMPLE_BLOCK_SIZE = (SAMPLE_BLOCK_SIZE/2) };
	enum { TX_SAMPLE_BLOCK_SIZE = (SAMPLE_BLOCK_SIZE*10) };
	enum { DATA_AND_PROBE_SYMBOLS_PER_FRAME = (SYMBOLS_PER_FRAME-PREAMBLE_LENGTH) };
	enum { SCRAMBLER_TABLE_LENGTH = DATA_AND_PROBE_SYMBOLS_PER_FRAME };	// since probes are also scrambled
	enum { MAX_PACKET_LENGTH = 10000 };
	enum { GOOD_PROBE_THRESHOLD = 1 };
	enum { BAD_PROBE_THRESHOLD =  2 };
	//enum { MODEM_SAMPLE_BLOCK_SIZE = 1192 };
	enum { INTERLEAVER_MAX_ARRAY_SIZE = 23840 };
	enum { INTERLEAVER_ENABLED = 0 };
	enum { INTERLEAVER_DISABLED = 1 };

	enum { RX_BLOCK_LENGTH = SAMPLE_BLOCK_SIZE };
	typedef void (*callbackRxOutput_t)(int arg, FComplex *samps, int nsamps, int incr);
	typedef unsigned char (*callbackTxInput_t)(void);

private:
	bool m_status_update;
	char m_status_text[256];
	unsigned int run_start, run_us;

	// Control variables
	DuplexMode duplex_mode;
	PttType    ptt_type;

	// Receive block vars
	int   bad_preamble_count;
	int   preamble_start;
	int   preamble_matches;
	int   data_start;
	int   rx_chan;
	doppler_t sync_delta;
	float accumulator;

	int rx_mode;
	int tx_mode;
	Kmode rx_mode_type;
	int con_rx_octets_per_block;
	int con_tx_flush_octets;
	int con_bad_probe_threshold;
	int con_tx_octets_per_block;
	int   m_preamble_errors;
	float m_frequency_error;
	float sample_rate;

	// TX variables
	FComplex tx_buffer[TX_FILTER_LENGTH/4];
	float    tx_acc;
	FComplex tx_Csamples[TX_SAMPLE_BLOCK_SIZE];
	float    tx_samples[TX_SAMPLE_BLOCK_SIZE];
	int      tx_sample_wa;
	int      tx_sample_tx_ra, tx_sample_rx_ra;
	int      tx_sample_tx_count, tx_sample_rx_count;

	// AGC
	double hold;

	// Interleaver variables
	int   in[INTERLEAVER_MAX_ARRAY_SIZE];
	float de[INTERLEAVER_MAX_ARRAY_SIZE];
	int   iip[INTERLEAVER_NR_ROWS];
	int   iop[INTERLEAVER_NR_ROWS];
	int   dip[INTERLEAVER_NR_ROWS];
	int   dop[INTERLEAVER_NR_ROWS];
	int   tx_row_inc;
	int   rx_row_inc;
	int   tx_array_size;
	int   rx_array_size;
	int   in_row;
	int   de_row;
	int   interleaver_status;
	int   deinterleaver_status;

	// Queue variables
	unsigned char input_queue[IO_QUEUE_LENGTH];
	unsigned char output_queue[IO_QUEUE_LENGTH];
	int in_head_pointer;
	int in_tail_pointer;
	int out_head_pointer;
	int out_tail_pointer;
	int in_full_flag;
	int in_empty_flag;
	int out_full_flag;
	int out_empty_flag;

	// Kalman variables
	FComplex d_eq[FF_EQ_LENGTH+FB_EQ_LENGTH];
	FComplex  c[KN_44];
	FComplex  f[KN_44];
	FComplex  g[KN_44];
	FComplex  u[KN_44][KN_44];
	FComplex  h[KN_44];
	float     d[KN_44];
	float     a[KN_44];
	float     y;
	float     q;
	float     E;

	// Transmit variables
	int       tx_scramble_count;
	int       tx_count;
	int       tx_bitcount;
	int       tx_tribit;
	callbackTxInput_t tx_callback;

	// Scrambler tables
	int      scrambler_sequence[SCRAMBLER_TABLE_LENGTH];
	FComplex scrambler_table[SCRAMBLER_TABLE_LENGTH];
	FComplex scrambler_train_table[SCRAMBLER_TABLE_LENGTH];
	int      rx_scramble_count;

	// Convolutional variables
	ParityLookup parity_lookup[128];
	int          encode_state;
	float        acm[NR_CONVOLUTIONAL_STATES];	// Accumulated distance
	int          path[NR_CONVOLUTIONAL_STATES][NORMAL_PATH_LENGTH];
	int          hp;
	int          path_length;
	float        m_viterbi_confidence;

	// Receive variables
	RxState rx_state;
	float   delta[RX_CHANNELS];
	float   acc[RX_CHANNELS];
	FComplex frate_b[RX_CHANNELS][SAMPLE_BLOCK_SIZE+RX_FILTER_SIZE];
	FComplex hrate_b[RX_CHANNELS][3*HALF_SAMPLE_BLOCK_SIZE];
	float    rx_block_buffer[RX_BLOCK_LENGTH];
	int      rx_block_buffer_index;
	callbackRxOutput_t rx_callback;
	int		rx_callback_arg;

	// Demodulator variables
	float sd[DATA_LENGTH*4*4];
	int   soft_index;
	float mse_magnitudes[MSE_HISTORY_LENGTH];
	float mse_average;
	int   mse_count;

	// Error strings
	char frequency_error[50];

	// Output Data
	int output_data[OUTPUT_DATA_LENGTH];
	int output_offset;

	unsigned int m_sample_counter;

public:
	// Constellation code
	enum { MAX_NR_CONSTELLATION_SYMBOLS = 500 };
	FComplex m_constellation_symbols[MAX_NR_CONSTELLATION_SYMBOLS];
	unsigned int m_constellation_offset;

private:
	FDemodulate (CSt4285::*demod)(FComplex);
	void check_fast_intr();

	// Control
	int get_tx_octets_per_block( void );
	void set_vox_ptt( void );
	void set_rts_ptt( void );
	void set_dtr_ptt( void );
	void ptt_transmit( void );
	void ptt_receive( void );
	void set_duplex( void );
	void set_simplex( void );
	int is_duplex( void );
	int is_simplex( void );
	void set_tx_mode( Kmode mode );
	void set_rx_mode( Kmode mode );
	Kmode set_rx_mode_text( char *mode );
	const char *get_rx_mode_text( void );
	void *allocate_context( void );
	void process_samples( float *in, unsigned int length );
	unsigned int get_data( int *data, int &length );
	void get_set_params( const char *cmd, char *resp );
	void status_string( void );

	// Convolution
	void parity( int state, int *bit1, int *bit2 );
	void code_generate(void);
	void convolutional_init( void );
	void convolutional_encode_reset( void );
	void viterbi_set_normal_path_length( void  );
	void viterbi_set_alternate_1_path_length( void  );
	void viterbi_set_alternate_2_path_length( void  );
	void viterbi_decode_reset( void );
	void convolutional_encode( int in, int *bit1, int *bit2 );
	int viterbi_decode( float metric1, float metric2 );

	// Demodulate
	void update_mse_average( FComplex error );
	float get_mse_divisor( void );
	FDemodulate demodulate_bpsk( FComplex symbol );
	FDemodulate demodulate_qpsk( FComplex symbol );
	FDemodulate demodulate_8psk( FComplex symbol );
	void demodulate_and_equalize( FComplex *in );

	// Interleaver
	void interleaver_configure( void );
	void interleaver_reset( void );
	void deinterleaver_reset( void );
	void interleaver_enable( void );
	void interleaver_disable( void );
	void deinterleaver_enable( void );
	void deinterleaver_disable( void );
	void interleaving_tx_2400_short( void );
	void interleaving_rx_2400_short( void );
	void interleaving_tx_2400_long( void );
	void interleaving_rx_2400_long( void );
	void interleaving_tx_1200_short( void );
	void interleaving_rx_1200_short( void );
	void interleaving_tx_1200_long( void );
	void interleaving_rx_1200_long( void );
	void interleaving_tx_600_75_short( void );
	void interleaving_rx_600_75_short( void );
	void interleaving_tx_600_75_long( void );
	void interleaving_rx_600_75_long( void );
	void sync_deinterleaver( void );
	int interleave( int input );
	void sync_interleaver( void );
	float deinterleave( float input );

	// Kalman
	void kalman_reset_coffs( void );
	void kalman_reset_ud( void );
	void kalman_reset( void );
	void kalman_init( void );
	void kalman_calculate( FComplex *x );
	void kalman_update( FComplex *data, FComplex error );
	FComplex equalize( FComplex *in );
	void update_fb( FComplex in );
	void equalize_init( void );
	void equalize_reset( void );
	FComplex equalize_train( FComplex *in, FComplex train );
	FDemodulate equalize_data( FComplex *in );

	// Receive
	void report_frequency_error( float delta, int channel );
	FComplex rx_filter( FComplex *in );
	float preamble_correlate( FComplex *in );
	int preamble_hunt( FComplex *in, float *mag );
	int preamble_check( FComplex *in );
	FComplex agc( FComplex in );
	int is_dcd_detected( void );
	void reset_receive( int mode );
	doppler_t doppler_error( FComplex *in );
	doppler_t initial_doppler_correct( FComplex *in , doppler_t *delta );
	doppler_t doppler_correct( FComplex *in , doppler_t *delta );
	int train_and_equalize_on_preamble( FComplex *in );
	void rx_downconvert( float *in, FComplex *outa, FComplex *outb, float *acc, float delta );
	void rx_final_downconvert( FComplex *in, float *delta );
	void process_rx_block( float *in );
	void process_rx_block( float *in, int length );

	// Tables
	void iterate_data_scrambler_sequence( int *state );
	void build_data_scrambler_array( void );
	void build_scrambler_table( void );
	void start( void );

	// Transmit
	void tx_symbol( FComplex symbol );
	void tx_preamble( void );
	void tx_probe( void );
	void tx_frame_state( void );
	void tx_bpsk( int bit );
	void tx_qpsk( int dibit );
	void tx_psk8( int tribit );
	void tx_octet( unsigned char octet );
	void tx_frame( unsigned char *data, int length );
	void tx_output_iq( FComplex *data, int length, float scale );
	void tx_output_real( short *data, int length, float scale );
	void transmit_callback( void );

	// Constellation
	void save_constellation_symbol( FComplex symbol );

public:
	CSt4285();
	void reset();
	~CSt4285();

	// Control
	bool control( void *in, int type );
	bool control( void *in, void *out, int type );
	bool get_status_text( char *text );
	void setSampleRate( float srate );

	// Rx Input
	void input( float *in, int length );
	void input( float *real, float *imag, int length );
	void input( int *in, int length );
	int process_rx_block_tx_loopback( void );
	void process_rx_block( short *in, int length, float scale );

	// Rx Output
	unsigned int getRxOutput( void *out, int &length, int type );
	void registerRxCallback( callbackRxOutput_t func, int arg );

	// Tx Input
	void registerTxCallback( callbackTxInput_t func );

	// Tx Output
	unsigned int getTxOutput( void *out, int length, int type, float scale );
};

extern CSt4285 m_CSt4285[MAX_RX_CHANS];

#endif
