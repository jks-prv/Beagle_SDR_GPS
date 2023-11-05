#!/usr/bin/perl

# Matthijs van Duin - Dutch & Dutch
#
# sudo apt install libinline-files-perl
#
# 4 nov 2023    Added D11 support

use v5.14;
use strict;
use warnings qw( FATAL all );
use utf8;
use Inline::Files;

binmode STDOUT, ':utf8'  or die;

package style;

use base 'Tie::Hash';

sub TIEHASH {
	my( $class, @attrs ) = @_;
	my $style = "\e[" . join( ';', @attrs ) . 'm';
	bless \$style, $class
}

sub FETCH {
	my( $this, $key ) = @_;
       	"$$this$key\e[m"
}

package main;

tie my %RED, style => 31;
tie my %GRN, style => 32;
tie my %YEL, style => 33;
tie my %DIM_BLK, style => 2;
tie my %BOLD_RED, style => 31, 1;
tie my %DIM_RED, style => 31, 2;
tie my %DIM_GRN, style => 32, 2;



# parse options

my $verbose = 0;

while( @ARGV && $ARGV[0] =~ s/^-v/-/ ) {
	++$verbose;
	shift if $ARGV[0] eq '-';
}
die "usage: show-pins.pl [-v]\nuse 'show-pins.pl|sort' to sort by P8/P9 pin number\nexit"  if @ARGV;

my $tda4 = 1;
my $am5 = 0;

my $pinmux = '/sys/kernel/debug/pinctrl/11c000.pinmux';
unless(-e "$pinmux/pinmux-pins") {
	$pinmux = '/sys/kernel/debug/pinctrl/11c000.pinctrl-pinctrl-single';
}
unless(-e "$pinmux/pinmux-pins") {
	$pinmux = '/sys/kernel/debug/pinctrl/4a003400.pinmux';
	$tda4 = 0;
	$am5 = 1;
}
unless(-e "$pinmux/pinmux-pins") {
	$pinmux = '/sys/kernel/debug/pinctrl/4a003400.pinmux-pinctrl-single';
	$tda4 = 0;
	$am5 = 1;
}
unless(-e "$pinmux/pinmux-pins") {
	$pinmux = '/sys/kernel/debug/pinctrl/44e10800.pinmux';
	$tda4 = 0;
	$am5 = 0;
}
unless(-e "$pinmux/pinmux-pins") {
	$pinmux = '/sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single';
	$tda4 = 0;
	$am5 = 0;
}
@ARGV = "$pinmux/pinmux-pins";

my @usage;
while( <> ) {
	local $SIG{__DIE__} = sub { die "@_$_\n" };
	/^pin/gc  or next;
	/\G +(0|[1-9]\d*)/gc or die;
	my $pin = 0 + $1;
	if(/\G +\((\w+)\.0\):/gc) {
		my $addr = hex $1;
		( $addr & 0x7ff ) == 4 * $pin  or die;
	} elsif(/\G +\(PIN(0|[1-9]\d*)\):/gc) {
		$1 == $pin  or die;
	} else {
		die;
	}
	next if /\G \(MUX UNCLAIMED\)/gc;
	/\G (\S+)/gc  or die;
	my @path = split m!:!, $1;
	s/^(.*)\.([^.]+)\z/$2\@$1/ for @path;
	my $path = join '/', @path;
	/\G \(GPIO UNCLAIMED\)/gc  or die;
	/\G function (\S+) group (\S+)\s*\z/gc && $1 eq $2  or die;
	$usage[ $pin ] = [ $path, $1 ];
}


my @data;
if($tda4) {
	@data = map { chomp; [ split /\t/ ] } grep /^[^#]/, <TDA4XX>;
} elsif($am5) {
	@data = map { chomp; [ split /\t/ ] } grep /^[^#]/, <AM57XX>;
} else {
	@data = map { chomp; [ split /\t+/ ] } grep /^[^#]/, <AM335X>;
}
	@ARGV = "$pinmux/pins";


while( <> ) {
	s/\s*\z//;
	/^pin / or next;

	my ($pin, $reg, $mux);
	my $addpre = $tda4 ? "11" : ($am5 ? "4a00" : "44e1");
	if($tda4) {
		/^pin (\d+) \(PIN\d+\) 0\:\? $addpre([0-9a-f]{4}) ([0-9a-f]{8}) pinctrl-single\z/ or die "parse error";
		$pin = $1;
		$reg = hex $2;
		$mux = hex $3;
	} elsif(/\d+:/) {
		/^pin (\d+) \(PIN\d+\) \S* $addpre([0-9a-f]{4}) ([0-9a-f]{8}) pinctrl-single\z/ or die "parse error";
		$pin = $1;
		$reg = hex $2;
		$mux = hex $3;
	} elsif(/PIN/) {
		/^pin (\d+) \(PIN\d+\) $addpre([0-9a-f]{4}) ([0-9a-f]{8}) pinctrl-single\z/ or die "parse error";
		$pin = $1;
		$reg = hex $2;
		$mux = hex $3;
	} else {
		/^pin (\d+) \($addpre([0-9a-f]{4})\.0\) 000([0-9a-f]{5}) pinctrl-single\z/ or die "parse error";
		$pin = $1;
		$reg = hex $2;
		$mux = hex $3;
	}
	my ($abc_ball, $zcz_ball, $label);
	if($tda4) {
		0xc000 + 4 * $pin == $reg  or die "sanity check failed";
		$abc_ball = $data[ $pin ][ 2 ] || "";
		$label = $data[ $pin ][ 1 ] // next;
	} elsif($am5) {
		0x3400 + 4 * $pin == $reg  or die "sanity check failed";
		$abc_ball = $data[ $pin ][ 2 ] || "";
		$label = $data[ $pin ][ 1 ] // next;
	} else {
		0x800 + 4 * $pin == $reg  or die "sanity check failed";
		$zcz_ball = $data[ $pin ][ 9 ] || "";
		$label = $data[ $pin ][ 8 ] // next;
	}


	my $boring = $label =~ s/^-// ? 2 : $label !~ /^P[89]\./;
        next if $boring > $verbose;

	my ($mux_s, $lock, $wu_evt, $wu_en, $slew, $tx, $rx, $pull, $st, $mode, $mode_i, $mode_s);
	if($tda4) {
		# BB AI-64 TDA4XX
		$mux_s = sprintf "$YEL{'%08x'}", $mux;
		$mux_s = sprintf "$DIM_BLK{'%08x'}", $mux if $mux == 0x08214007;    # post-reset value
		$lock = ( "  ..", "$RED{lock}" )[ $mux >> 31 & 1 ];
		$wu_evt = ( "  ", "$RED{wu}" )[ $mux >> 30 & 1 ];
		$wu_en = ( "    ", "$RED{wuen}" )[ $mux >> 29 & 1 ];
		$tx = ( "$GRN{tx}", "  " )[ $mux >> 21 & 1 ];
		$slew = ( " $DIM_BLK{nom}", "$GRN{fast}", "$YEL{slow}", "$RED{resv}" )[ $mux >> 19 & 3 ];
		$rx = ( "  ", "$GRN{rx}" )[ $mux >> 18 & 1 ];
		$pull = ( "$DIM_GRN{down}", " $DIM_RED{up} " )[ $mux >> 17 & 1 ];
		$pull = "    " if $mux >> 16 & 1;
		$st = ( "  ", "$YEL{st}" )[ $mux >> 14 & 1 ];
		if ($pin == 0) {    # different from all the others! (see J721E_registers1.pdf 6.403 CTRLMMR_PADCONFIG0 vs CTRLMMR_PADCONFIG1 for example)
			$mux_s = sprintf "$DIM_BLK{'%08x'}", $mux if $mux == 0x00004007;    # post-reset value
			$slew = "    ";
			$pull = "    ";
		}
		$mode = $mux & 0xf;
		$mode_s = sprintf "$DIM_RED{'m%-2d'}", $mode;
		$mode_s = sprintf "$GRN{'m7'} " if $mode == 7;   # GPIO
		$mode_s = sprintf "$DIM_BLK{'m15'}" if $mode == 15;   # bootstrap
		$mode_i = $mode + 3;
	} elsif($am5) {
		# BB AI AM57XX
		$mux_s = sprintf "$YEL{'%08x'}", $mux;
		$wu_evt = ( "  ", "$RED{wu}" )[ $mux >> 25 & 1 ];
		$wu_en = ( "    ", "$RED{wuen}" )[ $mux >> 24 & 1 ];
		$slew = ( "$DIM_BLK{fast}", "$RED{slow}" )[ $mux >> 19 & 1 ];
		$rx = ( "  ", "$GRN{rx}" )[ $mux >> 18 & 1 ];
		$pull = ( "$DIM_GRN{down}", " $DIM_RED{up} " )[ $mux >> 17 & 1 ];
		$pull = "    " if $mux >> 16 & 1;
		$mode = $mux & 0xf;
		$mode_s = sprintf "$DIM_RED{'m%-2d'}", $mode;
		$mode_s = sprintf "$GRN{'m14'}" if $mode == 14;   # GPIO
		$mode_s = sprintf "$DIM_BLK{'m15'}" if $mode == 15;   # bootstrap
		$mode_i = $mode + 3;
	} else {
		# BB AM335X
		$mux_s = sprintf "$YEL{'%08x'}", $mux;
		$slew = ( "$DIM_BLK{fast}", "$RED{slow}" )[ $mux >> 6 & 1 ];
		$rx = ( "  ", "$GRN{rx}" )[ $mux >> 5 & 1 ];
		$pull = ( "$DIM_GRN{down}", " $DIM_RED{up} " )[ $mux >> 4 & 1 ];
		$pull = "    " if $mux >> 3 & 1;
		$mode = $mux & 7;
		$mode_s = sprintf "$DIM_RED{'m%d'}", $mode;
		$mode_s = sprintf "$GRN{'m7'}" if $mode == 7;   # GPIO
		$mode_i = $mode;
	}

	my $function = $data[ $pin ][ $mode_i ];
	$function = "$BOLD_RED{INVALID}" if $function eq '-';

	if( $usage[ $pin ] ) {
		my( $dev, $group ) = @{ $usage[ $pin ] };
		$function = sprintf "%-16s", $function;
		$function = join ' ', $function, $YEL{$dev}, $DIM_BLK{"($group)"};
	} elsif( $function =~ /^gpio / || $boring > 1 ) {
		$function = $DIM_BLK{$function};
	}

	$reg |= (hex $addpre) << 16;
	if($tda4) {
		$function = qq/$mux_s $slew $tx $rx $st $pull $mode_s $function/;
		printf "%-24s $YEL{'%3s'} $GRN{'%4s'} $DIM_BLK{'@%06x'} %s\n", $label, $pin, $abc_ball, $reg, $function;
	} elsif($am5) {
		$function = qq/$mux_s $slew $rx $pull $mode_s $function/;
		printf "%-24s $YEL{'%3s'} $GRN{'%4s'} $DIM_BLK{'@%08x'} %s\n", $label, $pin, $abc_ball, $reg, $function;
	} else {
		$function = qq/$mux_s $slew $rx $pull $mode_s $function/;
		printf "%-32s $YEL{'%3s'} $GRN{'%3s'} $DIM_BLK{'@%08x'} %s\n", $label, $pin, $zcz_ball, $reg, $function;
	}
}

__TDA4XX__
# reg		label	ball	mode 0-14, bootstrap
0x0011C000		AC18	extintn							gpio0_0								
0x0011C004	P9.11	AC23	prg1_pru0_gpo0	prg1_pru0_gpi0	prg1_rgmii1_rd0	prg1_pwm3_a0	rgmii1_rd0	rmii1_rxd0		gpio0_1	gpmc0_be1n	rgmii7_rd0			mcasp6_aclkx		uart0_rxd	
0x0011C008	P9.13	AG22	prg1_pru0_gpo1	prg1_pru0_gpi1	prg1_rgmii1_rd1	prg1_pwm3_b0	rgmii1_rd1	rmii1_rxd1		gpio0_2	gpmc0_wait0	rgmii7_rd1			mcasp6_afsx		uart0_txd	
0x0011C00C	P8.17	AF22	prg1_pru0_gpo2	prg1_pru0_gpi2	prg1_rgmii1_rd2	prg1_pwm2_a0	rgmii1_rd2	rmii1_crs_dv		gpio0_3	gpmc0_wait1	rgmii7_rd2			mcasp6_axr0		uart1_rxd	
0x0011C010	P8.18	AJ23	prg1_pru0_gpo3	prg1_pru0_gpi3	prg1_rgmii1_rd3	prg1_pwm3_a2	rgmii1_rd3	rmii1_rx_er		gpio0_4	gpmc0_dir	rgmii7_rd3			mcasp6_axr1		uart1_txd	
0x0011C014	P8.22	AH23	prg1_pru0_gpo4	prg1_pru0_gpi4	prg1_rgmii1_rx_ctl	prg1_pwm2_b0	rgmii1_rx_ctl	rmii1_txd0		gpio0_5	gpmc0_csn2	rgmii7_rx_ctl			mcasp6_axr2	mcasp6_aclkr	uart2_rxd	
0x0011C018	P8.24	AD20	prg1_pru0_gpo5	prg1_pru0_gpi5		prg1_pwm3_b2		rmii1_tx_en		gpio0_6	gpmc0_wen				mcasp3_axr0			bootmode0
0x0011C01C	P8.34	AD22	prg1_pru0_gpo6	prg1_pru0_gpi6	prg1_rgmii1_rxc	prg1_pwm3_a1	rgmii1_rxc	rmii1_txd1	audio_ext_refclk0	gpio0_7	gpmc0_csn3	rgmii7_rxc			mcasp6_axr3	mcasp6_afsr	uart2_txd	
0x0011C020	P8.36	AE20	prg1_pru0_gpo7	prg1_pru0_gpi7	prg1_iep0_edc_latch_in1	prg1_pwm3_b1		audio_ext_refclk1	mcan4_tx	gpio0_8					mcasp3_axr1			
0x0011C024	P8.38b	AJ20	prg1_pru0_gpo8	prg1_pru0_gpi8		prg1_pwm2_a1		rmii5_rxd0	mcan4_rx	gpio0_9	gpmc0_oen_ren		vout0_data22		mcasp3_axr2			
0x0011C028	P9.23	AG20	prg1_pru0_gpo9	prg1_pru0_gpi9	prg1_uart0_ctsn	prg1_pwm3_tz_in	spi6_cs1	rmii5_rxd1		gpio0_10	gpmc0_advn_ale	prg1_iep0_edio_data_in_out28	vout0_data23		mcasp3_aclkx			
0x0011C02C	P8.37b	AD21	prg1_pru0_gpo10	prg1_pru0_gpi10	prg1_uart0_rtsn	prg1_pwm2_b1	spi6_cs2	rmii5_crs_dv		gpio0_11	gpmc0_be0n_cle	prg1_iep0_edio_data_in_out29	obsclk2		mcasp3_afsx			
0x0011C030	P9.26b	AF24	prg1_pru0_gpo11	prg1_pru0_gpi11	prg1_rgmii1_td0	prg1_pwm3_tz_out	rgmii1_td0		mcan4_tx	gpio0_12		rgmii7_td0	vout0_data16	vpfe0_data0	mcasp7_aclkx			
0x0011C034	P9.24b	AJ24	prg1_pru0_gpo12	prg1_pru0_gpi12	prg1_rgmii1_td1	prg1_pwm0_a0	rgmii1_td1		mcan4_rx	gpio0_13		rgmii7_td1	vout0_data17	vpfe0_data1	mcasp7_afsx			
0x0011C038	P8.08	AG24	prg1_pru0_gpo13	prg1_pru0_gpi13	prg1_rgmii1_td2	prg1_pwm0_b0	rgmii1_td2		mcan5_tx	gpio0_14		rgmii7_td2	vout0_data18	vpfe0_data2	mcasp7_axr0			
0x0011C03C	P8.07	AD24	prg1_pru0_gpo14	prg1_pru0_gpi14	prg1_rgmii1_td3	prg1_pwm0_a1	rgmii1_td3		mcan5_rx	gpio0_15		rgmii7_td3	vout0_data19	vpfe0_data3	mcasp7_axr1			
0x0011C040	P8.10	AC24	prg1_pru0_gpo15	prg1_pru0_gpi15	prg1_rgmii1_tx_ctl	prg1_pwm0_b1	rgmii1_tx_ctl		mcan6_tx	gpio0_16		rgmii7_tx_ctl	vout0_data20	vpfe0_data4	mcasp7_axr2	mcasp7_aclkr		
0x0011C044	P8.09	AE24	prg1_pru0_gpo16	prg1_pru0_gpi16	prg1_rgmii1_txc	prg1_pwm0_a2	rgmii1_txc		mcan6_rx	gpio0_17		rgmii7_txc	vout0_data21	vpfe0_data5	mcasp7_axr3	mcasp7_afsr		
# NB: 11c048 is not used
0x0011C048			
0x0011C04C	P9.42b	AJ21	prg1_pru0_gpo17	prg1_pru0_gpi17	prg1_iep0_edc_sync_out1	prg1_pwm0_b2		rmii5_txd1	mcan5_tx	gpio0_18				vpfe0_data6	mcasp3_axr3			
0x0011C050		AE21	prg1_pru0_gpo18	prg1_pru0_gpi18	prg1_iep0_edc_latch_in0	prg1_pwm0_tz_in		rmii5_rx_er	mcan5_rx	gpio0_19				vpfe0_data7	mcasp4_aclkx			
0x0011C054	P8.03	AH21	prg1_pru0_gpo19	prg1_pru0_gpi19	prg1_iep0_edc_sync_out0	prg1_pwm0_tz_out		rmii5_txd0	mcan6_tx	gpio0_20			vout0_extpclkin	vpfe0_pclk	mcasp4_afsx			
0x0011C058		AE22	prg1_pru1_gpo0	prg1_pru1_gpi0	prg1_rgmii2_rd0		rgmii2_rd0	rmii2_rxd0		gpio0_21	rgmii8_rd0		vout0_data0	vpfe0_hd	mcasp8_aclkx			
0x0011C05C		AG23	prg1_pru1_gpo1	prg1_pru1_gpi1	prg1_rgmii2_rd1		rgmii2_rd1	rmii2_rxd1		gpio0_22	rgmii8_rd1		vout0_data1	vpfe0_field	mcasp8_afsx			
0x0011C060		AF23	prg1_pru1_gpo2	prg1_pru1_gpi2	prg1_rgmii2_rd2	prg1_pwm2_a2	rgmii2_rd2	rmii2_crs_dv		gpio0_23	rgmii8_rd2		vout0_data2	vpfe0_vd	mcasp8_axr0	mcasp3_aclkr		
0x0011C064	P8.35a	AD23	prg1_pru1_gpo3	prg1_pru1_gpi3	prg1_rgmii2_rd3		rgmii2_rd3	rmii2_rx_er		gpio0_24	rgmii8_rd3	eqep1_a	vout0_data3	vpfe0_wen	mcasp8_axr1	mcasp3_afsr	timer_io2	
0x0011C068	P8.33a	AH24	prg1_pru1_gpo4	prg1_pru1_gpi4	prg1_rgmii2_rx_ctl	prg1_pwm2_b2	rgmii2_rx_ctl	rmii2_txd0		gpio0_25	rgmii8_rx_ctl	eqep1_b	vout0_data4	vpfe0_data13	mcasp8_axr2	mcasp8_aclkr	timer_io3	
0x0011C06C	P8.32a	AG21	prg1_pru1_gpo5	prg1_pru1_gpi5				rmii5_tx_en	mcan6_rx	gpio0_26	gpmc0_wpn	eqep1_s	vout0_data5		mcasp4_axr0		timer_io4	
0x0011C070		AE23	prg1_pru1_gpo6	prg1_pru1_gpi6	prg1_rgmii2_rxc		rgmii2_rxc	rmii2_txd1		gpio0_27	rgmii8_rxc		vout0_data6	vpfe0_data14	mcasp8_axr3	mcasp8_afsr	timer_io5	
0x0011C074	P9.17a	AC21	prg1_pru1_gpo7	prg1_pru1_gpi7	prg1_iep1_edc_latch_in1		spi6_cs0	rmii6_rx_er	mcan7_tx	gpio0_28			vout0_data7	vpfe0_data15	mcasp4_axr1		uart3_txd	
0x0011C078		Y23	prg1_pru1_gpo8	prg1_pru1_gpi8		prg1_pwm2_tz_out		rmii6_rxd0	mcan7_rx	gpio0_29	gpmc0_csn1		vout0_data8		mcasp4_axr2		uart3_rxd	
0x0011C07C	P8.21	AF21	prg1_pru1_gpo9	prg1_pru1_gpi9	prg1_uart0_rxd		spi6_cs3	rmii6_rxd1	mcan8_tx	gpio0_30	gpmc0_csn0	prg1_iep0_edio_data_in_out30	vout0_data9		mcasp4_axr3			
0x0011C080	P8.23	AB23	prg1_pru1_gpo10	prg1_pru1_gpi10	prg1_uart0_txd	prg1_pwm2_tz_in		rmii6_crs_dv	mcan8_rx	gpio0_31	gpmc0_clkout	prg1_iep0_edio_data_in_out31	vout0_data10	gpmc0_fclk_mux	mcasp5_aclkx			
0x0011C084	P8.31a	AJ25	prg1_pru1_gpo11	prg1_pru1_gpi11	prg1_rgmii2_td0		rgmii2_td0	rmii2_tx_en		gpio0_32	rgmii8_td0	eqep1_i	vout0_data11		mcasp9_aclkx			
0x0011C088	P8.05	AH25	prg1_pru1_gpo12	prg1_pru1_gpi12	prg1_rgmii2_td1	prg1_pwm1_a0	rgmii2_td1		mcan7_tx	gpio0_33	rgmii8_td1		vout0_data12		mcasp9_afsx			
0x0011C08C	P8.06	AG25	prg1_pru1_gpo13	prg1_pru1_gpi13	prg1_rgmii2_td2	prg1_pwm1_b0	rgmii2_td2		mcan7_rx	gpio0_34	rgmii8_td2		vout0_data13	vpfe0_data8	mcasp9_axr0	mcasp4_aclkr		
0x0011C090	P8.25	AH26	prg1_pru1_gpo14	prg1_pru1_gpi14	prg1_rgmii2_td3	prg1_pwm1_a1	rgmii2_td3		mcan8_tx	gpio0_35	rgmii8_td3		vout0_data14		mcasp9_axr1	mcasp4_afsr		
0x0011C094		AJ27	prg1_pru1_gpo15	prg1_pru1_gpi15	prg1_rgmii2_tx_ctl	prg1_pwm1_b1	rgmii2_tx_ctl		mcan8_rx	gpio0_36	rgmii8_tx_ctl		vout0_data15	vpfe0_data9	mcasp9_axr2	mcasp9_aclkr		
0x0011C098		AJ26	prg1_pru1_gpo16	prg1_pru1_gpi16	prg1_rgmii2_txc	prg1_pwm1_a2	rgmii2_txc			gpio0_37	rgmii8_txc	vout0_vp2_hsync	vout0_hsync		mcasp9_axr3	mcasp9_afsr	vout0_vp0_hsync	
0x0011C09C	P9.22a	AC22	prg1_pru1_gpo17	prg1_pru1_gpi17	prg1_iep1_edc_sync_out1	prg1_pwm1_b2	spi6_clk	rmii6_tx_en	prg1_ecap0_sync_out	gpio0_38		vout0_vp2_de	vout0_de	vpfe0_data10	mcasp5_afsx		vout0_vp0_de	bootmode1
0x0011C0A0	P9.21a	AJ22	prg1_pru1_gpo18	prg1_pru1_gpi18	prg1_iep1_edc_latch_in0	prg1_pwm1_tz_in	spi6_d0	rmii6_txd0	prg1_ecap0_sync_in	gpio0_39		vout0_vp2_vsync	vout0_vsync		mcasp5_axr0		vout0_vp0_vsync	
0x0011C0A4	P9.18a	AH22	prg1_pru1_gpo19	prg1_pru1_gpi19	prg1_iep1_edc_sync_out0	prg1_pwm1_tz_out	spi6_d1	rmii6_txd1	prg1_ecap0_in_apwm_out	gpio0_40			vout0_pclk		mcasp5_axr1			
0x0011C0A8		AD19	prg1_mdio0_mdio	spi1_cs2	i2c4_scl					gpio0_41			dss_fsync1	vpfe0_data11	mcasp5_axr2	mcasp5_aclkr	uart3_ctsn	
0x0011C0AC		AD18	prg1_mdio0_mdc	spi1_cs3	i2c4_sda			rmii_ref_clk		gpio0_42				vpfe0_data12	mcasp5_axr3	mcasp5_afsr	uart3_rtsn	
0x0011C0B0	P9.28b	AF28	prg0_pru0_gpo0	prg0_pru0_gpi0	prg0_rgmii1_rd0	prg0_pwm3_a0	rgmii3_rd0	rmii3_rxd1		gpio0_43					mcasp0_axr0			
0x0011C0B4	P9.30b	AE28	prg0_pru0_gpo1	prg0_pru0_gpi1	prg0_rgmii1_rd1	prg0_pwm3_b0	rgmii3_rd1	rmii3_rxd0		gpio0_44					mcasp0_axr1			
0x0011C0B8	P9.12	AE27	prg0_pru0_gpo2	prg0_pru0_gpi2	prg0_rgmii1_rd2	prg0_pwm2_a0	rgmii3_rd2	rmii3_crs_dv		gpio0_45	uart3_rxd				mcasp0_aclkr			
0x0011C0BC	P9.27a	AD26	prg0_pru0_gpo3	prg0_pru0_gpi3	prg0_rgmii1_rd3	prg0_pwm3_a2	rgmii3_rd3	rmii3_rx_er		gpio0_46	uart3_txd				mcasp0_afsr			
0x0011C0C0	P9.15	AD25	prg0_pru0_gpo4	prg0_pru0_gpi4	prg0_rgmii1_rx_ctl	prg0_pwm2_b0	rgmii3_rx_ctl	rmii3_txd1		gpio0_47					mcasp0_axr2			
0x0011C0C4	P8.04	AC29	prg0_pru0_gpo5	prg0_pru0_gpi5		prg0_pwm3_b2		rmii3_txd0		gpio0_48	gpmc0_ad0				mcasp0_axr3			bootmode2
0x0011C0C8		AE26	prg0_pru0_gpo6	prg0_pru0_gpi6	prg0_rgmii1_rxc	prg0_pwm3_a1	rgmii3_rxc	rmii3_tx_en		gpio0_49					mcasp0_axr4			
0x0011C0CC	P9.33b	AC28	prg0_pru0_gpo7	prg0_pru0_gpi7	prg0_iep0_edc_latch_in1	prg0_pwm3_b1	prg0_ecap0_sync_in		mcan9_tx	gpio0_50	gpmc0_ad1				mcasp0_axr5			
0x0011C0D0	P8.26	AC27	prg0_pru0_gpo8	prg0_pru0_gpi8		prg0_pwm2_a1			mcan9_rx	gpio0_51	gpmc0_ad2				mcasp0_axr6		uart6_rxd	
0x0011C0D4	P9.31b	AB26	prg0_pru0_gpo9	prg0_pru0_gpi9	prg0_uart0_ctsn	prg0_pwm3_tz_in	spi3_cs1	prg0_iep0_edio_data_in_out28	mcan10_tx	gpio0_52	gpmc0_ad3				mcasp0_aclkx		uart6_txd	
0x0011C0D8	P9.29b	AB25	prg0_pru0_gpo10	prg0_pru0_gpi10	prg0_uart0_rtsn	prg0_pwm2_b1	spi3_cs2	prg0_iep0_edio_data_in_out29	mcan10_rx	gpio0_53	gpmc0_ad4				mcasp0_afsx			
0x0011C0DC	P9.39b	AJ28	prg0_pru0_gpo11	prg0_pru0_gpi11	prg0_rgmii1_td0	prg0_pwm3_tz_out	rgmii3_td0			gpio0_54		clkout			mcasp0_axr7			
0x0011C0E0	P9.35b	AH27	prg0_pru0_gpo12	prg0_pru0_gpi12	prg0_rgmii1_td1	prg0_pwm0_a0	rgmii3_td1			gpio0_55			dss_fsync0		mcasp0_axr8			
0x0011C0E4	P9.36b	AH29	prg0_pru0_gpo13	prg0_pru0_gpi13	prg0_rgmii1_td2	prg0_pwm0_b0	rgmii3_td2			gpio0_56			dss_fsync2		mcasp0_axr9			
0x0011C0E8	P9.37b	AG28	prg0_pru0_gpo14	prg0_pru0_gpi14	prg0_rgmii1_td3	prg0_pwm0_a1	rgmii3_td3			gpio0_57	uart4_rxd				mcasp0_axr10			
0x0011C0EC	P9.38b	AG27	prg0_pru0_gpo15	prg0_pru0_gpi15	prg0_rgmii1_tx_ctl	prg0_pwm0_b1	rgmii3_tx_ctl			gpio0_58	uart4_txd		dss_fsync3		mcasp0_axr11			
0x0011C0F0	P8.12	AH28	prg0_pru0_gpo16	prg0_pru0_gpi16	prg0_rgmii1_txc	prg0_pwm0_a2	rgmii3_txc			gpio0_59			dss_fsync1		mcasp0_axr12			
0x0011C0F4	P8.11	AB24	prg0_pru0_gpo17	prg0_pru0_gpi17	prg0_iep0_edc_sync_out1	prg0_pwm0_b2	prg0_ecap0_sync_out			gpio0_60	gpmc0_ad5	obsclk1			mcasp0_axr13			bootmode7
0x0011C0F8	P8.15	AB29	prg0_pru0_gpo18	prg0_pru0_gpi18	prg0_iep0_edc_latch_in0	prg0_pwm0_tz_in	prg0_ecap0_in_apwm_out			gpio0_61	gpmc0_ad6				mcasp0_axr14			
0x0011C0FC	P8.16	AB28	prg0_pru0_gpo19	prg0_pru0_gpi19	prg0_iep0_edc_sync_out0	prg0_pwm0_tz_out				gpio0_62	gpmc0_ad7				mcasp0_axr15			
0x0011C100	P8.31b	AE29	prg0_pru1_gpo0	prg0_pru1_gpi0	prg0_rgmii2_rd0		rgmii4_rd0	rmii4_rxd0		gpio0_63	uart4_ctsn				mcasp1_axr0		uart5_rxd	
0x0011C104	P8.32b	AD28	prg0_pru1_gpo1	prg0_pru1_gpi1	prg0_rgmii2_rd1		rgmii4_rd1	rmii4_rxd1		gpio0_64	uart4_rtsn				mcasp1_axr1		uart5_txd	
0x0011C108	P8.43	AD27	prg0_pru1_gpo2	prg0_pru1_gpi2	prg0_rgmii2_rd2	prg0_pwm2_a2	rgmii4_rd2	rmii4_crs_dv		gpio0_65	gpmc0_a23				mcasp1_aclkr	mcasp1_axr10		
0x0011C10C	P8.44	AC25	prg0_pru1_gpo3	prg0_pru1_gpi3	prg0_rgmii2_rd3		rgmii4_rd3	rmii4_rx_er		gpio0_66					mcasp1_afsr	mcasp1_axr11		
0x0011C110	P8.41	AD29	prg0_pru1_gpo4	prg0_pru1_gpi4	prg0_rgmii2_rx_ctl	prg0_pwm2_b2	rgmii4_rx_ctl	rmii4_txd1		gpio0_67	gpmc0_a24				mcasp1_axr2			
0x0011C114	P8.42	AB27	prg0_pru1_gpo5	prg0_pru1_gpi5						gpio0_68	gpmc0_ad8				mcasp1_aclkx			bootmode6
0x0011C118	P8.39	AC26	prg0_pru1_gpo6	prg0_pru1_gpi6	prg0_rgmii2_rxc		rgmii4_rxc	rmii4_txd0		gpio0_69	gpmc0_a25				mcasp1_axr3			
0x0011C11C	P8.40	AA24	prg0_pru1_gpo7	prg0_pru1_gpi7	prg0_iep1_edc_latch_in1		spi3_cs0		mcan11_tx	gpio0_70	gpmc0_ad9				mcasp1_axr4		uart2_txd	
0x0011C120	P8.27	AA28	prg0_pru1_gpo8	prg0_pru1_gpi8		prg0_pwm2_tz_out			mcan11_rx	gpio0_71	gpmc0_ad10				mcasp1_afsx			
0x0011C124	P8.28	Y24	prg0_pru1_gpo9	prg0_pru1_gpi9	prg0_uart0_rxd		spi3_cs3		prg0_iep0_edio_data_in_out30	gpio0_72	gpmc0_ad11		dss_fsync3		mcasp1_axr5		uart8_rxd	
0x0011C128	P8.29	AA25	prg0_pru1_gpo10	prg0_pru1_gpi10	prg0_uart0_txd	prg0_pwm2_tz_in			prg0_iep0_edio_data_in_out31	gpio0_73	gpmc0_ad12	clkout			mcasp1_axr6		uart8_txd	
0x0011C12C	P8.30	AG26	prg0_pru1_gpo11	prg0_pru1_gpi11	prg0_rgmii2_td0		rgmii4_td0	rmii4_tx_en		gpio0_74	gpmc0_a26				mcasp1_axr7			
0x0011C130	P8.14	AF27	prg0_pru1_gpo12	prg0_pru1_gpi12	prg0_rgmii2_td1	prg0_pwm1_a0	rgmii4_td1			gpio0_75					mcasp1_axr8		uart8_ctsn	
0x0011C134	P8.20	AF26	prg0_pru1_gpo13	prg0_pru1_gpi13	prg0_rgmii2_td2	prg0_pwm1_b0	rgmii4_td2			gpio0_76					mcasp1_axr9		uart8_rtsn	
0x0011C138	P9.20b	AE25	prg0_pru1_gpo14	prg0_pru1_gpi14	prg0_rgmii2_td3	prg0_pwm1_a1	rgmii4_td3			gpio0_77					mcasp2_axr0		uart2_ctsn	
0x0011C13C	P9.19b	AF29	prg0_pru1_gpo15	prg0_pru1_gpi15	prg0_rgmii2_tx_ctl	prg0_pwm1_b1	rgmii4_tx_ctl			gpio0_78					mcasp2_axr1		uart2_rtsn	
0x0011C140	P8.45	AG29	prg0_pru1_gpo16	prg0_pru1_gpi16	prg0_rgmii2_txc	prg0_pwm1_a2	rgmii4_txc			gpio0_79					mcasp2_axr2			
0x0011C144	P8.46	Y25	prg0_pru1_gpo17	prg0_pru1_gpi17	prg0_iep1_edc_sync_out1	prg0_pwm1_b2	spi3_clk			gpio0_80	gpmc0_ad13				mcasp2_axr3			bootmode3
0x0011C148	P9.40b	AA26	prg0_pru1_gpo18	prg0_pru1_gpi18	prg0_iep1_edc_latch_in0	prg0_pwm1_tz_in	spi3_d0		mcan12_tx	gpio0_81	gpmc0_ad14				mcasp2_afsx		uart2_rxd	
0x0011C14C		AA29	prg0_pru1_gpo19	prg0_pru1_gpi19	prg0_iep1_edc_sync_out0	prg0_pwm1_tz_out	spi3_d1		mcan12_rx	gpio0_82	gpmc0_ad15				mcasp2_aclkx			
0x0011C150		Y26	prg0_mdio0_mdio		i2c5_scl				mcan13_tx	gpio0_83	gpmc0_a27		dss_fsync0		mcasp2_afsr	mcasp2_axr4		
0x0011C154		AA27	prg0_mdio0_mdc		i2c5_sda				mcan13_rx	gpio0_84	gpmc0_a0		dss_fsync2		mcasp2_aclkr	mcasp2_axr5		
0x0011C158		U23	rgmii5_tx_ctl	rmii7_crs_dv	i2c2_scl		vout1_data0	trc_clk	ehrpwm0_synci	gpio0_85	gpmc0_a1				mcasp10_aclkx			
0x0011C15C		U26	rgmii5_rx_ctl	rmii7_rx_er	i2c2_sda		vout1_data1	trc_ctl	ehrpwm0_synco	gpio0_86	gpmc0_a2				mcasp10_afsx			
0x0011C160		V28	rgmii5_td3	uart3_rxd		sync2_out	vout1_data2	trc_data0	ehrpwm_tzn_in0	gpio0_87	gpmc0_a3				mcasp10_axr0			
0x0011C164	P8.19	V29	rgmii5_td2	uart3_txd		sync3_out	vout1_data3	trc_data1	ehrpwm0_a	gpio0_88	gpmc0_a4				mcasp10_axr1			
0x0011C168	P8.13	V27	rgmii5_td1	rmii7_txd1	i2c3_scl		vout1_data4	trc_data2	ehrpwm0_b	gpio0_89	gpmc0_a5				mcasp11_aclkx			
0x0011C16C	P9.21b	U28	rgmii5_td0	rmii7_txd0	i2c3_sda		vout1_data5	trc_data3	ehrpwm1_a	gpio0_90	gpmc0_a6				mcasp11_afsx			
0x0011C170	P9.22b	U29	rgmii5_txc	rmii7_tx_en	i2c6_scl		vout1_data6	trc_data4	ehrpwm1_b	gpio0_91	gpmc0_a7				mcasp10_axr2			
0x0011C174		U25	rgmii5_rxc		i2c6_sda		vout1_data7	trc_data5	ehrpwm_tzn_in1	gpio0_92	gpmc0_a8				mcasp10_axr3		ehrpwm_soca	
0x0011C178	P9.14	U27	rgmii5_rd3	uart3_ctsn		uart6_rxd	vout1_data8	trc_data6	ehrpwm2_a	gpio0_93	gpmc0_a9				mcasp11_axr0			
0x0011C17C	P9.16	U24	rgmii5_rd2	uart3_rtsn		uart6_txd	vout1_data9	trc_data7	ehrpwm2_b	gpio0_94	gpmc0_a10				mcasp11_axr1			
0x0011C180		R23	rgmii5_rd1	rmii7_rxd1		uart6_ctsn	vout1_data10	trc_data8	ehrpwm_tzn_in2	gpio0_95	gpmc0_a11				mcasp11_axr2		ehrpwm_socb	
0x0011C184		T23	rgmii5_rd0	rmii7_rxd0		uart6_rtsn	vout1_data11	trc_data9		gpio0_96	gpmc0_a12				mcasp11_axr3			
0x0011C188		Y28	rgmii6_tx_ctl	rmii8_crs_dv			vout1_data12	trc_data10		gpio0_97	gpmc0_a13				mcasp10_aclkr			
0x0011C18C		V23	rgmii6_rx_ctl	rmii8_rx_er			vout1_data13	trc_data11	ehrpwm3_a	gpio0_98	gpmc0_a14				mcasp10_afsr			
0x0011C190		W23	rgmii6_td3	uart4_rxd		spi5_cs3	vout1_data14	trc_data12	ehrpwm3_b	gpio0_99	gpmc0_a15				mcasp11_aclkr			
0x0011C194		W28	rgmii6_td2	uart4_txd		spi5_cs2	vout1_data15	trc_data13	ehrpwm3_synci	gpio0_100	gpmc0_a16				mcasp11_afsr			
0x0011C198		V25	rgmii6_td1	rmii8_txd1		spi5_d0	vout1_vsync	trc_data14	ehrpwm3_synco	gpio0_101	gpmc0_a17	vout1_vp0_vsync			mcasp10_axr4			
0x0011C19C		W27	rgmii6_td0	rmii8_txd0		spi5_cs0	vout1_hsync	trc_data15	ehrpwm_tzn_in3	gpio0_102	gpmc0_a18	vout1_vp0_hsync			mcasp10_axr5			
0x0011C1A0		W29	rgmii6_txc	rmii8_tx_en		spi5_clk	vout1_pclk	trc_data16	ehrpwm4_a	gpio0_103	gpmc0_a19				mcasp10_axr6			
0x0011C1A4	P9.25b	W26	rgmii6_rxc			audio_ext_refclk2	vout1_de	trc_data17	ehrpwm4_b	gpio0_104	gpmc0_a20	vout1_vp0_de			mcasp10_axr7			
0x0011C1A8	P8.38a	Y29	rgmii6_rd3	uart4_ctsn		uart5_rxd	clkout	trc_data18	ehrpwm_tzn_in4	gpio0_105	gpmc0_a21				mcasp11_axr4			
0x0011C1AC	P8.37a	Y27	rgmii6_rd2	uart4_rtsn		uart5_txd		trc_data19	ehrpwm5_a	gpio0_106	gpmc0_a22				mcasp11_axr5			
0x0011C1B0		W24	rgmii6_rd1	rmii8_rxd1		spi5_d1	vout1_extpclkin	trc_data20	ehrpwm5_b	gpio0_107	gpmc0_be1n				mcasp11_axr6			
0x0011C1B4		W25	rgmii6_rd0	rmii8_rxd0		spi5_cs1	audio_ext_refclk3	trc_data21	ehrpwm_tzn_in5	gpio0_108	gpmc0_dir				mcasp11_axr7			
0x0011C1B8		V26	mdio0_mdio					trc_data22		gpio0_109	gpmc0_wait3							
0x0011C1BC		V24	mdio0_mdc					trc_data23		gpio0_110	gpmc0_wait2							
0x0011C1C0	P8.33b	AA2	spi0_cs0	uart0_rtsn						gpio0_111								
0x0011C1C4		Y4	spi0_cs1	cpts0_ts_comp	i2c3_scl			dp0_hpd	prg1_iep0_edio_outvalid	gpio0_112								
0x0011C1C8		AA1	spi0_clk	uart1_ctsn	i2c2_scl					gpio0_113								
0x0011C1CC		AB5	spi0_d0	uart1_rtsn	i2c2_sda					gpio0_114								
0x0011C1D0	P9.17b	AA3	spi0_d1		i2c6_scl					gpio0_115								
0x0011C1D4	P8.35b	Y3	spi1_cs0	uart0_ctsn		uart5_rxd			prg0_iep0_edio_outvalid	gpio0_116	prg0_iep0_edc_latch_in0							
0x0011C1D8		W4	spi1_cs1	cpts0_ts_sync	i2c3_sda	uart5_txd				gpio0_117								
0x0011C1DC	P9.26a	Y1	spi1_clk	uart5_ctsn	i2c4_sda	uart2_rxd				gpio0_118	prg0_iep0_edc_sync_out0							
0x0011C1E0	P9.24a	Y5	spi1_d0	uart5_rtsn	i2c4_scl	uart2_txd				gpio0_119	prg0_iep1_edc_latch_in0							
0x0011C1E4	P9.18b	Y2	spi1_d1		i2c6_sda					gpio0_120	prg0_iep1_edc_sync_out0							
0x0011C1E8		AB2	uart0_rxd				spi2_cs1			gpio0_121								
0x0011C1EC		AB3	uart0_txd				spi2_cs2		spi7_cs1	gpio0_122								
0x0011C1F0	P9.42a	AC2	uart0_ctsn	timer_io6	spi0_cs2	mcan2_rx	spi2_cs0	eqep0_a		gpio0_123								
0x0011C1F4	P9.27b	AB1	uart0_rtsn	timer_io7	spi0_cs3	mcan2_tx	spi2_clk	eqep0_b		gpio0_124								
0x0011C1F8		AA4	uart1_rxd						spi7_cs2	gpio0_125								
0x0011C1FC		AB4	uart1_txd					i3c0_sdapullen	spi7_cs3	gpio0_126								
0x0011C200	P9.25a	AC4	uart1_ctsn	mcan3_rx			spi2_d0	eqep0_s		gpio0_127								
0x0011C204	P9.41	AD5	uart1_rtsn	mcan3_tx			spi2_d1	eqep0_i		gpio1_0								
0x0011C208	P9.19a	W5	mcan0_rx				i2c2_scl			gpio1_1								
0x0011C20C	P9.20a	W6	mcan0_tx				i2c2_sda			gpio1_2								
0x0011C210		W3	mcan1_rx	uart6_ctsn	uart9_rxd	usb0_drvvbus	usb1_drvvbus			gpio1_3								
0x0011C214		V4	mcan1_tx	uart6_rtsn	uart9_txd	usb0_drvvbus	usb1_drvvbus			gpio1_4								
0x0011C218		W2	i3c0_scl	mmc2_sdcd	uart9_ctsn	mcan2_rx	i2c6_scl	dp0_hpd	pcie0_clkreqn	gpio1_5	uart6_rxd							
0x0011C21C		W1	i3c0_sda	mmc2_sdwp	uart9_rtsn	mcan2_tx	i2c6_sda		pcie1_clkreqn	gpio1_6	uart6_txd							
0x0011C220		AC5	i2c0_scl							gpio1_7								
0x0011C224		AA5	i2c0_sda							gpio1_8								
0x0011C228		Y6	i2c1_scl	cpts0_hw1tspush						gpio1_9								
0x0011C22C		AA6	i2c1_sda	cpts0_hw2tspush						gpio1_10								
0x0011C230	P9.28a	U2	ecap0_in_apwm_out	sync0_out	cpts0_rft_clk		spi2_cs3	i3c0_sdapullen	spi7_cs0	gpio1_11								
0x0011C234	P9.31a	U3	ext_refclk1	sync1_out					spi7_clk	gpio1_12								
0x0011C238	P9.30a	V6	timer_io0	ecap1_in_apwm_out	sysclkout0				spi7_d0	gpio1_13								bootmode4
0x0011C23C	P9.29a	V5	timer_io1	ecap2_in_apwm_out	obsclk0				spi7_d1	gpio1_14								bootmode5
0x0011C240		R26	mmc1_dat3	uart7_rxd						gpio1_15								
0x0011C244		R25	mmc1_dat2	uart7_txd						gpio1_16								
0x0011C248		P24	mmc1_dat1	uart7_ctsn	ecap0_in_apwm_out	timer_io0		uart4_rxd		gpio1_17								
0x0011C24C		R24	mmc1_dat0	uart7_rtsn	ecap1_in_apwm_out	timer_io1		uart4_txd		gpio1_18								
0x0011C250		P25	mmc1_clk	uart8_rxd			i2c4_scl			gpio1_19								
0x0011C254		R29	mmc1_cmd	uart8_txd			i2c4_sda			gpio1_20								
0x0011C258		P23	mmc1_sdcd	uart8_ctsn	uart0_dcdn	timer_io2		eqep2_i	pcie2_clkreqn	gpio1_21	prg0_iep0_edc_latch_in1							
0x0011C25C		R28	mmc1_sdwp	uart8_rtsn	uart0_dsrn	timer_io3	ecap2_in_apwm_out	eqep2_s	pcie3_clkreqn	gpio1_22	prg0_iep0_edc_sync_out1							
0x0011C260		T28	mmc2_dat3	uart9_rxd	cpts0_hw1tspush		i2c5_scl			gpio1_23								
0x0011C264		T29	mmc2_dat2	uart9_txd	cpts0_hw2tspush		i2c5_sda			gpio1_24								
0x0011C268		T27	mmc2_dat1	uart9_ctsn	uart0_dtrn	timer_io4	uart6_rxd	eqep2_a		gpio1_25	prg0_iep1_edc_latch_in1							
0x0011C26C		T24	mmc2_dat0	uart9_rtsn	uart0_rin	timer_io5	uart6_txd	eqep2_b		gpio1_26	prg0_iep1_edc_sync_out1							
0x0011C270		T26	mmc2_clk	usb0_drvvbus	usb1_drvvbus	timer_io6	i2c3_scl	uart3_rxd		gpio1_27								
0x0011C274		T25	mmc2_cmd	usb0_drvvbus	usb1_drvvbus	timer_io7	i2c3_sda	uart3_txd		gpio1_28								
0x0011C278		T6	resetstatz															
0x0011C27C		U1	porz_out															
0x0011C280		U4	soc_safety_errorn															
0x0011C284		V1	tdi															
0x0011C288		V3	tdo															
0x0011C28C		V2	tms															
0x0011C290		U6	usb0_drvvbus	usb1_drvvbus						gpio1_29								
0x0011C294		AD1	mlb0_mlbsp							gpio1_30								
0x0011C298		AC1	mlb0_mlbsn							gpio1_31								
0x0011C29C		AC3	mlb0_mlbdp							gpio1_32								
0x0011C2A0		AD3	mlb0_mlbdn							gpio1_33								
0x0011C2A4		AD2	mlb0_mlbcp							gpio1_34								
0x0011C2A8		AE2	mlb0_mlbcn							gpio1_35								
0x4301C000		E20	mcu_ospi0_clk	mcu_hyperbus0_ck						wkup_gpio0_16								
0x4301C004		C21	mcu_ospi0_lbclko	mcu_hyperbus0_ckn						wkup_gpio0_17								
0x4301C008		D21	mcu_ospi0_dqs	mcu_hyperbus0_rwds						wkup_gpio0_18								
0x4301C00C		D20	mcu_ospi0_d0	mcu_hyperbus0_dq0						wkup_gpio0_19								
0x4301C010		G19	mcu_ospi0_d1	mcu_hyperbus0_dq1						wkup_gpio0_20								
0x4301C014		G20	mcu_ospi0_d2	mcu_hyperbus0_dq2						wkup_gpio0_21								
0x4301C018		F20	mcu_ospi0_d3	mcu_hyperbus0_dq3						wkup_gpio0_22								
0x4301C01C		F21	mcu_ospi0_d4	mcu_hyperbus0_dq4						wkup_gpio0_23								
0x4301C020		E21	mcu_ospi0_d5	mcu_hyperbus0_dq5						wkup_gpio0_24								
0x4301C024		B22	mcu_ospi0_d6	mcu_hyperbus0_dq6						wkup_gpio0_25								
0x4301C028		G21	mcu_ospi0_d7	mcu_hyperbus0_dq7						wkup_gpio0_26								
0x4301C02C		F19	mcu_ospi0_csn0	mcu_hyperbus0_csn0						wkup_gpio0_27								
0x4301C030		E19	mcu_ospi0_csn1	mcu_hyperbus0_resetn						wkup_gpio0_28								
0x4301C034		F22	mcu_ospi1_clk							wkup_gpio0_29								
0x4301C038		A23	mcu_ospi1_lbclko	mcu_ospi0_csn2	mcu_hyperbus0_reseton				mcu_ospi0_reset_out0	wkup_gpio0_30								
0x4301C03C		B23	mcu_ospi1_dqs	mcu_ospi0_csn3	mcu_hyperbus0_intn				mcu_ospi0_ecc_fail	wkup_gpio0_31								
0x4301C040		D22	mcu_ospi1_d0							wkup_gpio0_32								
0x4301C044		G22	mcu_ospi1_d1				mcu_uart0_rxd	mcu_spi1_cs1		wkup_gpio0_33								
0x4301C048		D23	mcu_ospi1_d2				mcu_uart0_txd	mcu_spi1_cs2		wkup_gpio0_34								
0x4301C04C		C23	mcu_ospi1_d3				mcu_uart0_ctsn	mcu_spi0_cs1		wkup_gpio0_35								
0x4301C050		C22	mcu_ospi1_csn0							wkup_gpio0_36								
0x4301C054		E22	mcu_ospi1_csn1	mcu_hyperbus0_wpn	mcu_timer_io0	mcu_hyperbus0_csn1	mcu_uart0_rtsn	mcu_spi0_cs2	mcu_ospi0_reset_out1	wkup_gpio0_37								
0x4301C058		B27	mcu_rgmii1_tx_ctl	mcu_rmii1_crs_dv						wkup_gpio0_38								
0x4301C05C		C25	mcu_rgmii1_rx_ctl	mcu_rmii1_rx_er						wkup_gpio0_39								
0x4301C060		A28	mcu_rgmii1_td3	mcu_timer_io2		mcu_adc_ext_trigger0				wkup_gpio0_40								
0x4301C064		A27	mcu_rgmii1_td2	mcu_timer_io3		mcu_adc_ext_trigger1				wkup_gpio0_41								
0x4301C068		A26	mcu_rgmii1_td1	mcu_rmii1_txd1						wkup_gpio0_42								
0x4301C06C		B25	mcu_rgmii1_td0	mcu_rmii1_txd0						wkup_gpio0_43								
0x4301C070		B26	mcu_rgmii1_txc	mcu_rmii1_tx_en						wkup_gpio0_44								
0x4301C074		C24	mcu_rgmii1_rxc	mcu_rmii1_ref_clk						wkup_gpio0_45								
0x4301C078		A25	mcu_rgmii1_rd3	mcu_timer_io4						wkup_gpio0_46								
0x4301C07C		D24	mcu_rgmii1_rd2	mcu_timer_io5						wkup_gpio0_47								
0x4301C080		A24	mcu_rgmii1_rd1	mcu_rmii1_rxd1						wkup_gpio0_48								
0x4301C084		B24	mcu_rgmii1_rd0	mcu_rmii1_rxd0						wkup_gpio0_49								
0x4301C088		E23	mcu_mdio0_mdio							wkup_gpio0_50								
0x4301C08C		F23	mcu_mdio0_mdc							wkup_gpio0_51								
0x4301C090		E27	mcu_spi0_clk							wkup_gpio0_52								mcu_bootmode00
0x4301C094		E24	mcu_spi0_d0							wkup_gpio0_53								mcu_bootmode01
0x4301C098		E28	mcu_spi0_d1				mcu_timer_io0			wkup_gpio0_54								mcu_bootmode02
0x4301C09C		E25	mcu_spi0_cs0				mcu_timer_io1			wkup_gpio0_55								
0x4301C0A0		J29	wkup_uart0_rxd							wkup_gpio0_56								
0x4301C0A4		J28	wkup_uart0_txd							wkup_gpio0_57								
0x4301C0A8		D29	mcu_mcan0_tx							wkup_gpio0_58								
0x4301C0AC		C29	mcu_mcan0_rx							wkup_gpio0_59								
0x4301C0B0		F26	mcu_spi1_clk	mcu_spi1_clk						wkup_gpio0_0								mcu_bootmode03
0x4301C0B4		F25	mcu_spi1_d0	mcu_spi1_d0						wkup_gpio0_1								mcu_bootmode04
0x4301C0B8		F28	mcu_spi1_d1	mcu_spi1_d1						wkup_gpio0_2								mcu_bootmode05
0x4301C0BC		F27	mcu_spi1_cs0	mcu_spi1_cs0						wkup_gpio0_3								
0x4301C0C0		G25	mcu_mcan1_tx	mcu_mcan1_tx	mcu_spi0_cs3	mcu_adc_ext_trigger0				wkup_gpio0_4								
0x4301C0C4		G24	mcu_mcan1_rx	mcu_mcan1_rx	mcu_spi1_cs3	mcu_adc_ext_trigger1				wkup_gpio0_5								
0x4301C0C8		F29	wkup_uart0_ctsn	wkup_uart0_ctsn	mcu_cpts0_hw1tspush	mcu_i2c1_scl				wkup_gpio0_6								
0x4301C0CC		G28	wkup_uart0_rtsn	wkup_uart0_rtsn	mcu_cpts0_hw2tspush	mcu_i2c1_sda				wkup_gpio0_7								
0x4301C0D0		G27	mcu_i2c1_scl	mcu_i2c1_scl	mcu_cpts0_ts_sync	mcu_i3c1_scl	mcu_timer_io6			wkup_gpio0_8								
0x4301C0D4		G26	mcu_i2c1_sda	mcu_i2c1_sda	mcu_cpts0_ts_comp	mcu_i3c1_sda	mcu_timer_io7			wkup_gpio0_9								
0x4301C0D8		H26	mcu_ext_refclk0	mcu_ext_refclk0	mcu_uart0_txd	mcu_adc_ext_trigger0	mcu_cpts0_rft_clk	mcu_sysclkout0		wkup_gpio0_10								
0x4301C0DC		H27	mcu_obsclk0	mcu_obsclk0	mcu_uart0_rxd	mcu_adc_ext_trigger1	mcu_timer_io1	mcu_i3c1_sdapullen	mcu_clkout0	wkup_gpio0_11								
0x4301C0E0		G29	mcu_uart0_txd	mcu_spi0_cs1						wkup_gpio0_12								mcu_bootmode08
0x4301C0E4		H28	mcu_uart0_rxd	mcu_spi1_cs1						wkup_gpio0_13								mcu_bootmode09
0x4301C0E8		H29	mcu_uart0_ctsn	mcu_spi0_cs2						wkup_gpio0_14								mcu_bootmode06
0x4301C0EC		J27	mcu_uart0_rtsn	mcu_spi1_cs2						wkup_gpio0_15								mcu_bootmode07
0x4301C0F0		D26	mcu_i3c0_scl		mcu_uart0_ctsn		mcu_timer_io8			wkup_gpio0_60								
0x4301C0F4		D25	mcu_i3c0_sda		mcu_uart0_rtsn		mcu_timer_io9			wkup_gpio0_61								
0x4301C0F8		J25	wkup_i2c0_scl							wkup_gpio0_62								
0x4301C0FC		H24	wkup_i2c0_sda							wkup_gpio0_63								
0x4301C100		J26	mcu_i2c0_scl							wkup_gpio0_64								
0x4301C104		H25	mcu_i2c0_sda							wkup_gpio0_65								
0x4301C108		E26	mcu_i3c0_sdapullen							wkup_gpio0_66								
0x4301C10C		G23	pmic_power_en1					mcu_i3c1_sdapullen		wkup_gpio0_67								
0x4301C110		D27	mcu_safety_errorn															
0x4301C114		D28	mcu_resetz															
0x4301C118		C27	mcu_resetstatz															
0x4301C11C		B28	mcu_porz_out															
0x4301C120		E29	tck															
0x4301C124		F24	trstn															
0x4301C128		C26	emu0															
0x4301C12C		B29	emu1															
0x4301C130	P9.39a	K25	mcu_adc0_ain0															
0x4301C134	P9.40a	K26	mcu_adc0_ain1															
0x4301C138	P9.37a	K28	mcu_adc0_ain2															
0x4301C13C	P9.38a	L28	mcu_adc0_ain3															
0x4301C140	P9.33a	K24	mcu_adc0_ain4															
0x4301C144	P9.36a	K27	mcu_adc0_ain5															
0x4301C148	P9.35a	K29	mcu_adc0_ain6															
0x4301C14C		L29	mcu_adc0_ain7															
0x4301C150		N23	mcu_adc1_ain0															
0x4301C154		M25	mcu_adc1_ain1															
0x4301C158		L24	mcu_adc1_ain2															
0x4301C15C		L26	mcu_adc1_ain3															
0x4301C160		N24	mcu_adc1_ain4															
0x4301C164		M24	mcu_adc1_ain5															
0x4301C168		L25	mcu_adc1_ain6															
0x4301C16C		L27	mcu_adc1_ain7															
0x4301C170		C28	reset_reqz															
0x4301C174		J24	porz															

__AM57XX__
# reg	label	ball	mode 0-14, bootstrap
0x1400	sysboot 0	M6	gpmc_ad0		vin3a_d0	vout3_d0											gpio1_6	sysboot0
0x1404	sysboot 1	M2	gpmc_ad1		vin3a_d1	vout3_d1											gpio1_7	sysboot1
0x1408	sysboot 2	L5	gpmc_ad2		vin3a_d2	vout3_d2											gpio1_8	sysboot2
0x140C	sysboot 3	M1	gpmc_ad3		vin3a_d3	vout3_d3											gpio1_9	sysboot3
0x1410	sysboot 4	L6	gpmc_ad4		vin3a_d4	vout3_d4											gpio1_10	sysboot4
0x1414	sysboot 5	L4	gpmc_ad5		vin3a_d5	vout3_d5											gpio1_11	sysboot5
0x1418	sysboot 6	L3	gpmc_ad6		vin3a_d6	vout3_d6											gpio1_12	sysboot6
0x141C	sysboot 7	L2	gpmc_ad7		vin3a_d7	vout3_d7											gpio1_13	sysboot7
0x1420	sysboot 8	L1	gpmc_ad8		vin3a_d8	vout3_d8											gpio7_18	sysboot8
0x1424	sysboot 9	K2	gpmc_ad9		vin3a_d9	vout3_d9											gpio7_19	sysboot9
0x1428	sysboot 10	J1	gpmc_ad10		vin3a_d10	vout3_d10											gpio7_28	sysboot10
0x142C	sysboot 11	J2	gpmc_ad11		vin3a_d11	vout3_d11											gpio7_29	sysboot11
0x1430	sysboot 12	H1	gpmc_ad12		vin3a_d12	vout3_d12											gpio1_18	sysboot12
0x1434	sysboot 13	J3	gpmc_ad13		vin3a_d13	vout3_d13											gpio1_19	sysboot13
0x1438	sysboot 14	H2	gpmc_ad14		vin3a_d14	vout3_d14											gpio1_20	sysboot14
0x143C	sysboot 15	H3	gpmc_ad15		vin3a_d15	vout3_d15											gpio1_21	sysboot15
0x1440	P9.19a	R6	gpmc_a0		vin3a_d16	vout3_d16	vin4a_d0		vin4b_d0	i2c4_scl	uart5_rxd						gpio7_3	Driver off
0x1444	P9.20a	T9	gpmc_a1		vin3a_d17	vout3_d17	vin4a_d1		vin4b_d1	i2c4_sda	uart5_txd						gpio7_4	Driver off
0x1448		T6	gpmc_a2		vin3a_d18	vout3_d18	vin4a_d2		vin4b_d2	uart7_rxd	uart5_ctsn						gpio7_5	Driver off
0x144C		T7	gpmc_a3	qspi1_cs2	vin3a_d19	vout3_d19	vin4a_d3		vin4b_d3	uart7_txd	uart5_rtsn						gpio7_6	Driver off
0x1450	bluetooth in	P6	gpmc_a4	qspi1_cs3	vin3a_d20	vout3_d20	vin4a_d4		vin4b_d4	i2c5_scl	uart6_rxd						gpio1_26	Driver off
0x1454	bluetooth out	R9	gpmc_a5		vin3a_d21	vout3_d21	vin4a_d5		vin4b_d5	i2c5_sda	uart6_txd						gpio1_27	Driver off
0x1458	bluetooth cts	R5	gpmc_a6		vin3a_d22	vout3_d22	vin4a_d6		vin4b_d6	uart8_rxd	uart6_ctsn						gpio1_28	Driver off
0x145C	bluetooth rts	P5	gpmc_a7		vin3a_d23	vout3_d23	vin4a_d7		vin4b_d7	uart8_txd	uart6_rtsn						gpio1_29	Driver off
0x1460		N7	gpmc_a8		vin3a_hsync0	vout3_hsync			vin4b_hsync1	timer12	spi4_sclk						gpio1_30	Driver off
0x1464		R4	gpmc_a9		vin3a_vsync0	vout3_vsync			vin4b_vsync1	timer11	spi4_d1						gpio1_31	Driver off
0x1468		N9	gpmc_a10		vin3a_de0	vout3_de			vin4b_clk1	timer10	spi4_d0						gpio2_0	Driver off
0x146C		P9	gpmc_a11		vin3a_fld0	vout3_fld	vin4a_fld0		vin4b_de1	timer9	spi4_cs0						gpio2_1	Driver off
0x1470		P4	gpmc_a12				vin4a_clk0	gpmc_a0	vin4b_fld1	timer8	spi4_cs1	dma_evt1					gpio2_2	Driver off
0x1474		R3	gpmc_a13	qspi1_rtclk			vin4a_hsync0			timer7	spi4_cs2	dma_evt2					gpio2_3	Driver off
0x1478		T2	gpmc_a14	qspi1_d3			vin4a_vsync0			timer6	spi4_cs3						gpio2_4	Driver off
0x147C		U2	gpmc_a15	qspi1_d2			vin4a_d8			timer5							gpio2_5	Driver off
0x1480		U1	gpmc_a16	qspi1_d0			vin4a_d9										gpio2_6	Driver off
0x1484		P3	gpmc_a17	qspi1_d1			vin4a_d10										gpio2_7	Driver off
0x1488		R2	gpmc_a18	qspi1_sclk			vin4a_d11										gpio2_8	Driver off
0x148C	eMMC d4	K7	gpmc_a19	mmc2_dat4	gpmc_a13		vin4a_d12		vin3b_d0								gpio2_9	Driver off
0x1490	eMMC d5	M7	gpmc_a20	mmc2_dat5	gpmc_a14		vin4a_d13		vin3b_d1								gpio2_10	Driver off
0x1494	eMMC d6	J5	gpmc_a21	mmc2_dat6	gpmc_a15		vin4a_d14		vin3b_d2								gpio2_11	Driver off
0x1498	eMMC d7	K6	gpmc_a22	mmc2_dat7	gpmc_a16		vin4a_d15		vin3b_d3								gpio2_12	Driver off
0x149C	eMMC clk	J7	gpmc_a23	mmc2_clk	gpmc_a17		vin4a_fld0		vin3b_d4								gpio2_13	Driver off
0x14A0	eMMC d0	J4	gpmc_a24	mmc2_dat0	gpmc_a18				vin3b_d5								gpio2_14	Driver off
0x14A4	eMMC d1	J6	gpmc_a25	mmc2_dat1	gpmc_a19				vin3b_d6								gpio2_15	Driver off
0x14A8	eMMC d2	H4	gpmc_a26	mmc2_dat2	gpmc_a20				vin3b_d7								gpio2_16	Driver off
0x14AC	eMMC d3	H5	gpmc_a27	mmc2_dat3	gpmc_a21				vin3b_hsync1								gpio2_17	Driver off
0x14B0	eMMC cmd	H6	gpmc_cs1	mmc2_cmd	gpmc_a22		vin4a_de0		vin3b_vsync1								gpio2_18	Driver off
0x14B4		T1	gpmc_cs0														gpio2_19	Driver off
0x14B8		P2	gpmc_cs2	qspi1_cs0													gpio2_20	Driver off
0x14BC		P1	gpmc_cs3	qspi1_cs1	vin3a_clk0	vout3_clk		gpmc_a1									gpio2_21	Driver off
0x14C0		P7	gpmc_clk	gpmc_cs7	clkout1	gpmc_wait1	vin4a_hsync0	vin4a_de0	vin3b_clk1	timer4	i2c3_scl	dma_evt1					gpio2_22	Driver off
0x14C4		N1	gpmc_advn_ale	gpmc_cs6	clkout2	gpmc_wait1	vin4a_vsync0	gpmc_a2	gpmc_a23	timer3	i2c3_sda	dma_evt2					gpio2_23	Driver off
0x14C8		M5	gpmc_oen_ren														gpio2_24	Driver off
0x14CC		M3	gpmc_wen														gpio2_25	Driver off
0x14D0		N6	gpmc_ben0	gpmc_cs4		vin1b_hsync1			vin3b_de1	timer2		dma_evt3					gpio2_26	Driver off
0x14D4		M4	gpmc_ben1	gpmc_cs5		vin1b_de1	vin3b_clk1	gpmc_a3	vin3b_fld1	timer1		dma_evt4					gpio2_27	Driver off
0x14D8		N2	gpmc_wait0														gpio2_28	Driver off
0x14DC	adc irq	AG8	vin1a_clk0			vout3_d16	vout3_fld										gpio2_30	Driver off
0x14E0		AH7	vin1b_clk1						vin3a_clk0								gpio2_31	Driver off
0x14E4	P8.35b	AD9	vin1a_de0	vin1b_hsync1		vout3_d17	vout3_de	uart7_rxd		timer16	spi3_sclk	kbd_row0	eQEP1A_in				gpio3_0	Driver off
0x14E8	P8.33b	AF9	vin1a_fld0	vin1b_vsync1			vout3_clk	uart7_txd		timer15	spi3_d1	kbd_row1	eQEP1B_in				gpio3_1	Driver off
0x14EC		AE9	vin1a_hsync0	vin1b_fld1			vout3_hsync	uart7_ctsn		timer14	spi3_d0		eQEP1_index				gpio3_2	Driver off
0x14F0	P9.21a	AF8	vin1a_vsync0	vin1b_de1			vout3_vsync	uart7_rtsn		timer13	spi3_cs0		eQEP1_strobe				gpio3_3	Driver off
0x14F4		AE8	vin1a_d0			vout3_d7	vout3_d23	uart8_rxd					ehrpwm1A				gpio3_4	Driver off
0x14F8		AD8	vin1a_d1			vout3_d6	vout3_d22	uart8_txd					ehrpwm1B				gpio3_5	Driver off
0x14FC		AG7	vin1a_d2			vout3_d5	vout3_d21	uart8_ctsn					ehrpwm1_tripzone_input				gpio3_6	Driver off
0x1500	user led 4	AH6	vin1a_d3			vout3_d4	vout3_d20	uart8_rtsn					eCAP1_in_PWM1_out		pr1_pru0_gpi0	pr1_pru0_gpo0	gpio3_7	Driver off
0x1504		AH3	vin1a_d4			vout3_d3	vout3_d19						ehrpwm1_synci		pr1_pru0_gpi1	pr1_pru0_gpo1	gpio3_8	Driver off
0x1508		AH5	vin1a_d5			vout3_d2	vout3_d18						ehrpwm1_synco		pr1_pru0_gpi2	pr1_pru0_gpo2	gpio3_9	Driver off
0x150C	P8.12	AG6	vin1a_d6			vout3_d1	vout3_d17						eQEP2A_in		pr1_pru0_gpi3	pr1_pru0_gpo3	gpio3_10	Driver off
0x1510	P8.11	AH4	vin1a_d7			vout3_d0	vout3_d16						eQEP2B_in		pr1_pru0_gpi4	pr1_pru0_gpo4	gpio3_11	Driver off
0x1514	P9.15	AG4	vin1a_d8	vin1b_d7			vout3_d15					kbd_row2	eQEP2_index		pr1_pru0_gpi5	pr1_pru0_gpo5	gpio3_12	Driver off
0x1518	usb C id	AG2	vin1a_d9	vin1b_d6			vout3_d14					kbd_row3	eQEP2_strobe		pr1_pru0_gpi6	pr1_pru0_gpo6	gpio3_13	Driver off
0x151C	user led 3	AG3	vin1a_d10	vin1b_d5			vout3_d13					kbd_row4	pr1_edc_latch0_in		pr1_pru0_gpi7	pr1_pru0_gpo7	gpio3_14	Driver off
0x1520	user led 2	AG5	vin1a_d11	vin1b_d4			vout3_d12	gpmc_a23				kbd_row5	pr1_edc_latch1_in		pr1_pru0_gpi8	pr1_pru0_gpo8	gpio3_15	Driver off
0x1524		AF2	vin1a_d12	vin1b_d3			vout3_d11	gpmc_a24				kbd_row6	pr1_edc_sync0_out		pr1_pru0_gpi9	pr1_pru0_gpo9	gpio3_16	Driver off
0x1528	user led 0	AF6	vin1a_d13	vin1b_d2			vout3_d10	gpmc_a25				kbd_row7	pr1_edc_sync1_out		pr1_pru0_gpi10	pr1_pru0_gpo10	gpio3_17	Driver off
0x152C	wifi reg on	AF3	vin1a_d14	vin1b_d1			vout3_d9	gpmc_a26				kbd_row8	pr1_edio_latch_in		pr1_pru0_gpi11	pr1_pru0_gpo11	gpio3_18	Driver off
0x1530		AF4	vin1a_d15	vin1b_d0			vout3_d8	gpmc_a27				kbd_col0	pr1_edio_sof		pr1_pru0_gpi12	pr1_pru0_gpo12	gpio3_19	Driver off
0x1534	bluetooth host wake	AF1	vin1a_d16	vin1b_d7			vout3_d7		vin3a_d0			kbd_col1	pr1_edio_data_in0	pr1_edio_data_out0	pr1_pru0_gpi13	pr1_pru0_gpo13	gpio3_20	Driver off
0x1538	bluetooth wake	AE3	vin1a_d17	vin1b_d6			vout3_d6		vin3a_d1			kbd_col2	pr1_edio_data_in1	pr1_edio_data_out1	pr1_pru0_gpi14	pr1_pru0_gpo14	gpio3_21	Driver off
0x153C	bluetooth reg on	AE5	vin1a_d18	vin1b_d5			vout3_d5		vin3a_d2			kbd_col3	pr1_edio_data_in2	pr1_edio_data_out2	pr1_pru0_gpi15	pr1_pru0_gpo15	gpio3_22	Driver off
0x1540	wifi host wake	AE1	vin1a_d19	vin1b_d4			vout3_d4		vin3a_d3			kbd_col4	pr1_edio_data_in3	pr1_edio_data_out3	pr1_pru0_gpi16	pr1_pru0_gpo16	gpio3_23	Driver off
0x1544	P9.26b	AE2	vin1a_d20	vin1b_d3			vout3_d3		vin3a_d4			kbd_col5	pr1_edio_data_in4	pr1_edio_data_out4	pr1_pru0_gpi17	pr1_pru0_gpo17	gpio3_24	Driver off
0x1548	usb C irq	AE6	vin1a_d21	vin1b_d2			vout3_d2		vin3a_d5			kbd_col6	pr1_edio_data_in5	pr1_edio_data_out5	pr1_pru0_gpi18	pr1_pru0_gpo18	gpio3_25	Driver off
0x154C		AD2	vin1a_d22	vin1b_d1			vout3_d1		vin3a_d6			kbd_col7	pr1_edio_data_in6	pr1_edio_data_out6	pr1_pru0_gpi19	pr1_pru0_gpo19	gpio3_26	Driver off
0x1550	adc voltage select	AD3	vin1a_d23	vin1b_d0			vout3_d0		vin3a_d7			kbd_col8	pr1_edio_data_in7	pr1_edio_data_out7	pr1_pru0_gpi20	pr1_pru0_gpo20	gpio3_27	Driver off
0x1554		E1	vin2a_clk0				vout2_fld	emu5				kbd_row0	eQEP1A_in		pr1_edio_data_in0	pr1_edio_data_out0	gpio3_28	Driver off
0x1558		G2	vin2a_de0	vin2a_fld0	vin2b_fld1	vin2b_de1	vout2_de	emu6				kbd_row1	eQEP1B_in		pr1_edio_data_in1	pr1_edio_data_out1	gpio3_29	Driver off
0x155C		H7	vin2a_fld0		vin2b_clk1		vout2_clk	emu7					eQEP1_index		pr1_edio_data_in2	pr1_edio_data_out2	gpio3_30	Driver off
0x1560		G1	vin2a_hsync0			vin2b_hsync1	vout2_hsync	emu8		uart9_rxd	spi4_sclk	kbd_row2	eQEP1_strobe	pr1_uart0_cts_n	pr1_edio_data_in3	pr1_edio_data_out3	gpio3_31	Driver off
0x1564	P8.34b	G6	vin2a_vsync0			vin2b_vsync1	vout2_vsync	emu9		uart9_txd	spi4_d1	kbd_row3	ehrpwm1A	pr1_uart0_rts_n	pr1_edio_data_in4	pr1_edio_data_out4	gpio4_0	Driver off
0x1568	P8.36b	F2	vin2a_d0				vout2_d23	emu10		uart9_ctsn	spi4_d0	kbd_row4	ehrpwm1B	pr1_uart0_rxd	pr1_edio_data_in5	pr1_edio_data_out5	gpio4_1	Driver off
0x156C		F3	vin2a_d1				vout2_d22	emu11		uart9_rtsn	spi4_cs0	kbd_row5	ehrpwm1_tripzone_input	pr1_uart0_txd	pr1_edio_data_in6	pr1_edio_data_out6	gpio4_2	Driver off
0x1570	P8.15a	D1	vin2a_d2				vout2_d21	emu12			uart10_rxd	kbd_row6	eCAP1_in_PWM1_out	pr1_ecap0_ecap_capin_apwm_o	pr1_edio_data_in7	pr1_edio_data_out7	gpio4_3	Driver off
0x1574		E2	vin2a_d3				vout2_d20	emu13			uart10_txd	kbd_col0	ehrpwm1_synci	pr1_edc_latch0_in	pr1_pru1_gpi0	pr1_pru1_gpo0	gpio4_4	Driver off
0x1578	P9.20b	D2	vin2a_d4				vout2_d19	emu14			uart10_ctsn	kbd_col1	ehrpwm1_synco	pr1_edc_sync0_out	pr1_pru1_gpi1	pr1_pru1_gpo1	gpio4_5	Driver off
0x157C	P9.19b	F4	vin2a_d5				vout2_d18	emu15			uart10_rtsn	kbd_col2	eQEP2A_in	pr1_edio_sof	pr1_pru1_gpi2	pr1_pru1_gpo2	gpio4_6	Driver off
0x1580	P9.41b	C1	vin2a_d6				vout2_d17	emu16			mii1_rxd1	kbd_col3	eQEP2B_in	pr1_mii_mt1_clk	pr1_pru1_gpi3	pr1_pru1_gpo3	gpio4_7	Driver off
0x1584		E4	vin2a_d7				vout2_d16	emu17			mii1_rxd2	kbd_col4	eQEP2_index	pr1_mii1_txen	pr1_pru1_gpi4	pr1_pru1_gpo4	gpio4_8	Driver off
0x1588	P8.18	F5	vin2a_d8				vout2_d15	emu18			mii1_rxd3	kbd_col5	eQEP2_strobe	pr1_mii1_txd3	pr1_pru1_gpi5	pr1_pru1_gpo5	gpio4_9	Driver off
0x158C	P8.19	E6	vin2a_d9				vout2_d14	emu19			mii1_rxd0	kbd_col6	ehrpwm2A	pr1_mii1_txd2	pr1_pru1_gpi6	pr1_pru1_gpo6	gpio4_10	Driver off
0x1590	P8.13	D3	vin2a_d10			mdio_mclk	vout2_d13					kbd_col7	ehrpwm2B	pr1_mdio_mdclk	pr1_pru1_gpi7	pr1_pru1_gpo7	gpio4_11	Driver off
0x1594		F6	vin2a_d11			mdio_d	vout2_d12					kbd_row7	ehrpwm2_tripzone_input	pr1_mdio_data	pr1_pru1_gpi8	pr1_pru1_gpo8	gpio4_12	Driver off
0x1598	P8.14	D5	vin2a_d12			rgmii1_txc	vout2_d11				mii1_rxclk	kbd_col8	eCAP2_in_PWM2_out	pr1_mii1_txd1	pr1_pru1_gpi9	pr1_pru1_gpo9	gpio4_13	Driver off
0x159C	P9.42b	C2	vin2a_d13			rgmii1_txctl	vout2_d10				mii1_rxdv	kbd_row8	eQEP3A_in	pr1_mii1_txd0	pr1_pru1_gpi10	pr1_pru1_gpo10	gpio4_14	Driver off
0x15A0	P9.27a	C3	vin2a_d14			rgmii1_txd3	vout2_d9				mii1_txclk		eQEP3B_in	pr1_mii_mr1_clk	pr1_pru1_gpi11	pr1_pru1_gpo11	gpio4_15	Driver off
0x15A4		C4	vin2a_d15			rgmii1_txd2	vout2_d8				mii1_txd0		eQEP3_index	pr1_mii1_rxdv	pr1_pru1_gpi12	pr1_pru1_gpo12	gpio4_16	Driver off
0x15A8		B2	vin2a_d16		vin2b_d7	rgmii1_txd1	vout2_d7		vin3a_d8		mii1_txd1		eQEP3_strobe	pr1_mii1_rxd3	pr1_pru1_gpi13	pr1_pru1_gpo13	gpio4_24	Driver off
0x15AC	P9.14	D6	vin2a_d17		vin2b_d6	rgmii1_txd0	vout2_d6		vin3a_d9		mii1_txd2		ehrpwm3A	pr1_mii1_rxd2	pr1_pru1_gpi14	pr1_pru1_gpo14	gpio4_25	Driver off
0x15B0	P9.16	C5	vin2a_d18		vin2b_d5	rgmii1_rxc	vout2_d5		vin3a_d10		mii1_txd3		ehrpwm3B	pr1_mii1_rxd1	pr1_pru1_gpi15	pr1_pru1_gpo15	gpio4_26	Driver off
0x15B4	P8.15b	A3	vin2a_d19		vin2b_d4	rgmii1_rxctl	vout2_d4		vin3a_d11		mii1_txer		ehrpwm3_tripzone_input	pr1_mii1_rxd0	pr1_pru1_gpi16	pr1_pru1_gpo16	gpio4_27	Driver off
0x15B8	P8.26	B3	vin2a_d20		vin2b_d3	rgmii1_rxd3	vout2_d3	vin3a_de0	vin3a_d12		mii1_rxer		eCAP3_in_PWM3_out	pr1_mii1_rxer	pr1_pru1_gpi17	pr1_pru1_gpo17	gpio4_28	Driver off
0x15BC	P8.16	B4	vin2a_d21		vin2b_d2	rgmii1_rxd2	vout2_d2	vin3a_fld0	vin3a_d13		mii1_col			pr1_mii1_rxlink	pr1_pru1_gpi18	pr1_pru1_gpo18	gpio4_29	Driver off
0x15C0		B5	vin2a_d22		vin2b_d1	rgmii1_rxd1	vout2_d1	vin3a_hsync0	vin3a_d14		mii1_crs			pr1_mii1_col	pr1_pru1_gpi19	pr1_pru1_gpo19	gpio4_30	Driver off
0x15C4		A4	vin2a_d23		vin2b_d0	rgmii1_rxd0	vout2_d0	vin3a_vsync0	vin3a_d15		mii1_txen			pr1_mii1_crs	pr1_pru1_gpi20	pr1_pru1_gpo20	gpio4_31	Driver off
0x15C8	P8.28a	D11	vout1_clk			vin4a_fld0	vin3a_fld0				spi3_cs0						gpio4_19	Driver off
0x15CC	P8.30a	B10	vout1_de			vin4a_de0	vin3a_de0				spi3_d1						gpio4_20	Driver off
0x15D0		B11	vout1_fld			vin4a_clk0	vin3a_clk0				spi3_cs1						gpio4_21	Driver off
0x15D4	P8.29a	C11	vout1_hsync			vin4a_hsync0	vin3a_hsync0				spi3_d0						gpio4_22	Driver off
0x15D8	P8.27a	E11	vout1_vsync			vin4a_vsync0	vin3a_vsync0				spi3_sclk				pr2_pru1_gpi17	pr2_pru1_gpo17	gpio4_23	Driver off
0x15DC	P8.45a	F11	vout1_d0		uart5_rxd	vin4a_d16	vin3a_d16				spi3_cs2		pr1_uart0_cts_n		pr2_pru1_gpi18	pr2_pru1_gpo18	gpio8_0	Driver off
0x15E0	P8.46a	G10	vout1_d1		uart5_txd	vin4a_d17	vin3a_d17						pr1_uart0_rts_n		pr2_pru1_gpi19	pr2_pru1_gpo19	gpio8_1	Driver off
0x15E4	P8.43	F10	vout1_d2		emu2	vin4a_d18	vin3a_d18	obs0	obs16	obs_irq1			pr1_uart0_rxd		pr2_pru1_gpi20	pr2_pru1_gpo20	gpio8_2	Driver off
0x15E8	P8.44	G11	vout1_d3		emu5	vin4a_d19	vin3a_d19	obs1	obs17	obs_dmarq1			pr1_uart0_txd		pr2_pru0_gpi0	pr2_pru0_gpo0	gpio8_3	Driver off
0x15EC	P8.41	E9	vout1_d4		emu6	vin4a_d20	vin3a_d20	obs2	obs18				pr1_ecap0_ecap_capin_apwm_o		pr2_pru0_gpi1	pr2_pru0_gpo1	gpio8_4	Driver off
0x15F0	P8.42	F9	vout1_d5		emu7	vin4a_d21	vin3a_d21	obs3	obs19				pr2_edc_latch0_in		pr2_pru0_gpi2	pr2_pru0_gpo2	gpio8_5	Driver off
0x15F4	P8.39	F8	vout1_d6		emu8	vin4a_d22	vin3a_d22	obs4	obs20				pr2_edc_latch1_in		pr2_pru0_gpi3	pr2_pru0_gpo3	gpio8_6	Driver off
0x15F8	P8.40	E7	vout1_d7		emu9	vin4a_d23	vin3a_d23						pr2_edc_sync0_out		pr2_pru0_gpi4	pr2_pru0_gpo4	gpio8_7	Driver off
0x15FC	P8.37a	E8	vout1_d8		uart6_rxd	vin4a_d8	vin3a_d8						pr2_edc_sync1_out		pr2_pru0_gpi5	pr2_pru0_gpo5	gpio8_8	Driver off
0x1600	P8.38a	D9	vout1_d9		uart6_txd	vin4a_d9	vin3a_d9						pr2_edio_latch_in		pr2_pru0_gpi6	pr2_pru0_gpo6	gpio8_9	Driver off
0x1604	P8.36a	D7	vout1_d10		emu3	vin4a_d10	vin3a_d10	obs5	obs21	obs_irq2			pr2_edio_sof		pr2_pru0_gpi7	pr2_pru0_gpo7	gpio8_10	Driver off
0x1608	P8.34a	D8	vout1_d11		emu10	vin4a_d11	vin3a_d11	obs6	obs22	obs_dmarq2			pr2_uart0_cts_n		pr2_pru0_gpi8	pr2_pru0_gpo8	gpio8_11	Driver off
0x160C	P8.35a	A5	vout1_d12		emu11	vin4a_d12	vin3a_d12	obs7	obs23				pr2_uart0_rts_n		pr2_pru0_gpi9	pr2_pru0_gpo9	gpio8_12	Driver off
0x1610	P8.33a	C6	vout1_d13		emu12	vin4a_d13	vin3a_d13	obs8	obs24				pr2_uart0_rxd		pr2_pru0_gpi10	pr2_pru0_gpo10	gpio8_13	Driver off
0x1614	P8.31a	C8	vout1_d14		emu13	vin4a_d14	vin3a_d14	obs9	obs25				pr2_uart0_txd		pr2_pru0_gpi11	pr2_pru0_gpo11	gpio8_14	Driver off
0x1618	P8.32a	C7	vout1_d15		emu14	vin4a_d15	vin3a_d15	obs10	obs26				pr2_ecap0_ecap_capin_apwm_o		pr2_pru0_gpi12	pr2_pru0_gpo12	gpio8_15	Driver off
0x161C	P8.45b	B7	vout1_d16		uart7_rxd	vin4a_d0	vin3a_d0						pr2_edio_data_in0	pr2_edio_data_out0	pr2_pru0_gpi13	pr2_pru0_gpo13	gpio8_16	Driver off
0x1620	P9.11b	B8	vout1_d17		uart7_txd	vin4a_d1	vin3a_d1						pr2_edio_data_in1	pr2_edio_data_out1	pr2_pru0_gpi14	pr2_pru0_gpo14	gpio8_17	Driver off
0x1624	P8.17	A7	vout1_d18		emu4	vin4a_d2	vin3a_d2	obs11	obs27				pr2_edio_data_in2	pr2_edio_data_out2	pr2_pru0_gpi15	pr2_pru0_gpo15	gpio8_18	Driver off
0x1628	P8.27b	A8	vout1_d19		emu15	vin4a_d3	vin3a_d3	obs12	obs28				pr2_edio_data_in3	pr2_edio_data_out3	pr2_pru0_gpi16	pr2_pru0_gpo16	gpio8_19	Driver off
0x162C	P8.28b	C9	vout1_d20		emu16	vin4a_d4	vin3a_d4	obs13	obs29				pr2_edio_data_in4	pr2_edio_data_out4	pr2_pru0_gpi17	pr2_pru0_gpo17	gpio8_20	Driver off
0x1630	P8.29b	A9	vout1_d21		emu17	vin4a_d5	vin3a_d5	obs14	obs30				pr2_edio_data_in5	pr2_edio_data_out5	pr2_pru0_gpi18	pr2_pru0_gpo18	gpio8_21	Driver off
0x1634	P8.30b	B9	vout1_d22		emu18	vin4a_d6	vin3a_d6	obs15	obs31				pr2_edio_data_in6	pr2_edio_data_out6	pr2_pru0_gpi19	pr2_pru0_gpo19	gpio8_22	Driver off
0x1638	P8.46b	A10	vout1_d23		emu19	vin4a_d7	vin3a_d7				spi3_cs3		pr2_edio_data_in7	pr2_edio_data_out7	pr2_pru0_gpi20	pr2_pru0_gpo20	gpio8_23	Driver off
0x163C	eth mdio clk	V1	mdio_mclk	uart3_rtsn		mii0_col	vin2a_clk0	vin4b_clk1						pr1_mii0_col	pr2_pru1_gpi0	pr2_pru1_gpo0	gpio5_15	Driver off
0x1640	eth mdio data	U4	mdio_d	uart3_ctsn		mii0_txer	vin2a_d0	vin4b_d0						pr1_mii0_rxlink	pr2_pru1_gpi1	pr2_pru1_gpo1	gpio5_16	Driver off
0x1644		U3	RMII_MHZ_50_CLK				vin2a_d11								pr2_pru1_gpi2	pr2_pru1_gpo2	gpio5_17	Driver off
0x1648	usb A vbus overcurrent	V2	uart3_rxd		rmii1_crs	mii0_rxdv	vin2a_d1	vin4b_d1		spi3_sclk				pr1_mii0_rxdv	pr2_pru1_gpi3	pr2_pru1_gpo3	gpio5_18	Driver off
0x164C	eth mii irq	Y1	uart3_txd		rmii1_rxer	mii0_rxclk	vin2a_d2	vin4b_d2		spi3_d1	spi4_cs1			pr1_mii_mr0_clk	pr2_pru1_gpi4	pr2_pru1_gpo4	gpio5_19	Driver off
0x1650	eth mii tx clk	W9	rgmii0_txc	uart3_ctsn	rmii1_rxd1	mii0_rxd3	vin2a_d3	vin4b_d3		spi3_d0	spi4_cs2			pr1_mii0_rxd3	pr2_pru1_gpi5	pr2_pru1_gpo5	gpio5_20	Driver off
0x1654	eth mii tx en	V9	rgmii0_txctl	uart3_rtsn	rmii1_rxd0	mii0_rxd2	vin2a_d4	vin4b_d4		spi3_cs0	spi4_cs3			pr1_mii0_rxd2	pr2_pru1_gpi6	pr2_pru1_gpo6	gpio5_21	Driver off
0x1658	eth mii tx d3	V7	rgmii0_txd3	rmii0_crs		mii0_crs	vin2a_de0	vin4b_de1		spi4_sclk	uart4_rxd			pr1_mii0_crs	pr2_pru1_gpi7	pr2_pru1_gpo7	gpio5_22	Driver off
0x165C	eth mii tx d2	U7	rgmii0_txd2	rmii0_rxer		mii0_rxer	vin2a_hsync0	vin4b_hsync1		spi4_d1	uart4_txd			pr1_mii0_rxer	pr2_pru1_gpi8	pr2_pru1_gpo8	gpio5_23	Driver off
0x1660	eth mii tx d1	V6	rgmii0_txd1	rmii0_rxd1		mii0_rxd1	vin2a_vsync0	vin4b_vsync1		spi4_d0	uart4_ctsn			pr1_mii0_rxd1	pr2_pru1_gpi9	pr2_pru1_gpo9	gpio5_24	Driver off
0x1664	eth mii tx d0	U6	rgmii0_txd0	rmii0_rxd0		mii0_rxd0	vin2a_d10			spi4_cs0	uart4_rtsn			pr1_mii0_rxd0	pr2_pru1_gpi10	pr2_pru1_gpo10	gpio5_25	Driver off
0x1668	eth mii rx clk	U5	rgmii0_rxc		rmii1_txen	mii0_txclk	vin2a_d5	vin4b_d5						pr1_mii_mt0_clk	pr2_pru1_gpi11	pr2_pru1_gpo11	gpio5_26	Driver off
0x166C	eth mii rx en	V5	rgmii0_rxctl		rmii1_txd1	mii0_txd3	vin2a_d6	vin4b_d6						pr1_mii0_txd3	pr2_pru1_gpi12	pr2_pru1_gpo12	gpio5_27	Driver off
0x1670	eth mii rx d3	V4	rgmii0_rxd3		rmii1_txd0	mii0_txd2	vin2a_d7	vin4b_d7						pr1_mii0_txd2	pr2_pru1_gpi13	pr2_pru1_gpo13	gpio5_28	Driver off
0x1674	eth mii rx d2	V3	rgmii0_rxd2	rmii0_txen		mii0_txen	vin2a_d8							pr1_mii0_txen	pr2_pru1_gpi14	pr2_pru1_gpo14	gpio5_29	Driver off
0x1678	eth mii rx d1	Y2	rgmii0_rxd1	rmii0_txd1		mii0_txd1	vin2a_d9							pr1_mii0_txd1	pr2_pru1_gpi15	pr2_pru1_gpo15	gpio5_30	Driver off
0x167C	eth mii rx d3	W2	rgmii0_rxd0	rmii0_txd0		mii0_txd0	vin2a_fld0	vin4b_fld1						pr1_mii0_txd0	pr2_pru1_gpi16	pr2_pru1_gpo16	gpio5_31	Driver off
0x1680	P9.13b	AB10	usb1_drvvbus							timer16							gpio6_12	Driver off
0x1684	usb A vbus out en	AC10	usb2_drvvbus							timer15							gpio6_13	Driver off
0x1688	P9.26a	E21	gpio6_14	mcasp1_axr8	dcan2_tx	uart10_rxd			vout2_hsync		vin4a_hsync0	i2c3_sda	timer1				gpio6_14	Driver off
0x168C	P9.24	F20	gpio6_15	mcasp1_axr9	dcan2_rx	uart10_txd			vout2_vsync		vin4a_vsync0	i2c3_scl	timer2				gpio6_15	Driver off
0x1690	pmic irq	F21	gpio6_16	mcasp1_axr10					vout2_fld		vin4a_fld0	clkout1	timer3				gpio6_16	Driver off
0x1694	P9.25	D18	xref_clk0	mcasp2_axr8	mcasp1_axr4	mcasp1_ahclkx	mcasp5_ahclkx			vin6a_d0	hdq0	clkout2	timer13	pr2_mii1_col	pr2_pru1_gpi5	pr2_pru1_gpo5	gpio6_17	Driver off
0x1698	P8.09	E17	xref_clk1	mcasp2_axr9	mcasp1_axr5	mcasp2_ahclkx	mcasp6_ahclkx			vin6a_clk0			timer14	pr2_mii1_crs	pr2_pru1_gpi6	pr2_pru1_gpo6	gpio6_18	Driver off
0x169C	P9.22a	B26	xref_clk2	mcasp2_axr10	mcasp1_axr6	mcasp3_ahclkx	mcasp7_ahclkx		vout2_clk		vin4a_clk0		timer15				gpio6_19	Driver off
0x16A0	P9.41a	C23	xref_clk3	mcasp2_axr11	mcasp1_axr7	mcasp4_ahclkx	mcasp8_ahclkx		vout2_de	hdq0	vin4a_de0	clkout3	timer16				gpio6_20	Driver off
0x16A4	P9.31b	C14	mcasp1_aclkx							vin6a_fld0			i2c3_sda	pr2_mdio_mdclk	pr2_pru1_gpi7	pr2_pru1_gpo7	gpio7_31	Driver off
0x16A8	P9.29b	D14	mcasp1_fsx							vin6a_de0			i2c3_scl	pr2_mdio_data			gpio7_30	Driver off
0x16AC	P9.12	B14	mcasp1_aclkr	mcasp7_axr2					vout2_d0		vin4a_d0		i2c4_sda				gpio5_0	Driver off
0x16B0	P9.27b	J14	mcasp1_fsr	mcasp7_axr3					vout2_d1		vin4a_d1		i2c4_scl				gpio5_1	Driver off
0x16B4	P9.18b	G12	mcasp1_axr0			uart6_rxd				vin6a_vsync0			i2c5_sda	pr2_mii0_rxer	pr2_pru1_gpi8	pr2_pru1_gpo8	gpio5_2	Driver off
0x16B8	P9.17b	F12	mcasp1_axr1			uart6_txd				vin6a_hsync0			i2c5_scl	pr2_mii_mt0_clk	pr2_pru1_gpi9	pr2_pru1_gpo9	gpio5_3	Driver off
0x16BC		G13	mcasp1_axr2	mcasp6_axr2		uart6_ctsn			vout2_d2		vin4a_d2						gpio5_4	Driver off
0x16C0	user led 1	J11	mcasp1_axr3	mcasp6_axr3		uart6_rtsn			vout2_d3		vin4a_d3						gpio5_5	Driver off
0x16C4		E12	mcasp1_axr4	mcasp4_axr2					vout2_d4		vin4a_d4						gpio5_6	Driver off
0x16C8	eMMC reset	F13	mcasp1_axr5	mcasp4_axr3					vout2_d5		vin4a_d5						gpio5_7	Driver off
0x16CC		C12	mcasp1_axr6	mcasp5_axr2					vout2_d6		vin4a_d6						gpio5_8	Driver off
0x16D0		D12	mcasp1_axr7	mcasp5_axr3					vout2_d7		vin4a_d7		timer4				gpio5_9	Driver off
0x16D4	P9.31a	B12	mcasp1_axr8	mcasp6_axr0		spi3_sclk				vin6a_d15			timer5	pr2_mii0_txen	pr2_pru1_gpi10	pr2_pru1_gpo10	gpio5_10	Driver off
0x16D8	P9.29a	A11	mcasp1_axr9	mcasp6_axr1		spi3_d1				vin6a_d14			timer6	pr2_mii0_txd3	pr2_pru1_gpi11	pr2_pru1_gpo11	gpio5_11	Driver off
0x16DC	P9.30	B13	mcasp1_axr10	mcasp6_aclkx	mcasp6_aclkr	spi3_d0				vin6a_d13			timer7	pr2_mii0_txd2	pr2_pru1_gpi12	pr2_pru1_gpo12	gpio5_12	Driver off
0x16E0	P9.28	A12	mcasp1_axr11	mcasp6_fsx	mcasp6_fsr	spi3_cs0				vin6a_d12			timer8	pr2_mii0_txd1	pr2_pru1_gpi13	pr2_pru1_gpo13	gpio4_17	Driver off
0x16E4	P9.42a	E14	mcasp1_axr12	mcasp7_axr0		spi3_cs1				vin6a_d11			timer9	pr2_mii0_txd0	pr2_pru1_gpi14	pr2_pru1_gpo14	gpio4_18	Driver off
0x16E8	P8.10	A13	mcasp1_axr13	mcasp7_axr1						vin6a_d10			timer10	pr2_mii_mr0_clk	pr2_pru1_gpi15	pr2_pru1_gpo15	gpio6_4	Driver off
0x16EC	P8.07	G14	mcasp1_axr14	mcasp7_aclkx	mcasp7_aclkr					vin6a_d9			timer11	pr2_mii0_rxdv	pr2_pru1_gpi16	pr2_pru1_gpo16	gpio6_5	Driver off
0x16F0	P8.08	F14	mcasp1_axr15	mcasp7_fsx	mcasp7_fsr					vin6a_d8			timer12	pr2_mii0_rxd3	pr2_pru0_gpi20	pr2_pru0_gpo20	gpio6_6	Driver off
0x16F4	bluetooth audio aclkx	A19	mcasp2_aclkx							vin6a_d7				pr2_mii0_rxd2	pr2_pru0_gpi18	pr2_pru0_gpo18		Driver off
0x16F8	bluetooth audio fsx	A18	mcasp2_fsx							vin6a_d6				pr2_mii0_rxd1	pr2_pru0_gpi19	pr2_pru0_gpo19		Driver off
0x16FC		E15	mcasp2_aclkr	mcasp8_axr2					vout2_d8		vin4a_d8							Driver off
0x1700		A20	mcasp2_fsr	mcasp8_axr3					vout2_d9		vin4a_d9							Driver off
0x1704	bluetooth audio pcm in	B15	mcasp2_axr0						vout2_d10		vin4a_d10							Driver off
0x1708	bluetooth audio pcm out	A15	mcasp2_axr1						vout2_d11		vin4a_d11							Driver off
0x170C		C15	mcasp2_axr2	mcasp3_axr2						vin6a_d5				pr2_mii0_rxd0	pr2_pru0_gpi16	pr2_pru0_gpo16	gpio6_8	Driver off
0x1710		A16	mcasp2_axr3	mcasp3_axr3						vin6a_d4				pr2_mii0_rxlink	pr2_pru0_gpi17	pr2_pru0_gpo17	gpio6_9	Driver off
0x1714		D15	mcasp2_axr4	mcasp8_axr0					vout2_d12		vin4a_d12						gpio1_4	Driver off
0x1718		B16	mcasp2_axr5	mcasp8_axr1					vout2_d13		vin4a_d13						gpio6_7	Driver off
0x171C		B17	mcasp2_axr6	mcasp8_aclkx	mcasp8_aclkr				vout2_d14		vin4a_d14						gpio2_29	Driver off
0x1720		A17	mcasp2_axr7	mcasp8_fsx	mcasp8_fsr				vout2_d15		vin4a_d15						gpio1_5	Driver off
0x1724		B18	mcasp3_aclkx	mcasp3_aclkr	mcasp2_axr12	uart7_rxd				vin6a_d3				pr2_mii0_crs	pr2_pru0_gpi12	pr2_pru0_gpo12	gpio5_13	Driver off
0x1728		F15	mcasp3_fsx	mcasp3_fsr	mcasp2_axr13	uart7_txd				vin6a_d2				pr2_mii0_col	pr2_pru0_gpi13	pr2_pru0_gpo13	gpio5_14	Driver off
0x172C	P9.11a	B19	mcasp3_axr0		mcasp2_axr14	uart7_ctsn	uart5_rxd			vin6a_d1				pr2_mii1_rxer	pr2_pru0_gpi14	pr2_pru0_gpo14		Driver off
0x1730	P9.13a	C17	mcasp3_axr1		mcasp2_axr15	uart7_rtsn	uart5_txd			vin6a_d0		vin5a_fld0		pr2_mii1_rxlink	pr2_pru0_gpi15	pr2_pru0_gpo15		Driver off
0x1734	P8.38b	C18	mcasp4_aclkx	mcasp4_aclkr	spi3_sclk	uart8_rxd	i2c4_sda		vout2_d16		vin4a_d16	vin5a_d15						Driver off
0x1738	P8.37b	A21	mcasp4_fsx	mcasp4_fsr	spi3_d1	uart8_txd	i2c4_scl		vout2_d17		vin4a_d17	vin5a_d14						Driver off
0x173C	P8.31b	G16	mcasp4_axr0		spi3_d0	uart8_ctsn	uart4_rxd		vout2_d18		vin4a_d18	vin5a_d13						Driver off
0x1740	P8.32b	D17	mcasp4_axr1		spi3_cs0	uart8_rtsn	uart4_txd		vout2_d19		vin4a_d19	vin5a_d12			pr2_pru1_gpi0	pr2_pru1_gpo0		Driver off
0x1744		AA3	mcasp5_aclkx	mcasp5_aclkr	spi4_sclk	uart9_rxd	i2c5_sda		vout2_d20		vin4a_d20	vin5a_d11			pr2_pru1_gpi1	pr2_pru1_gpo1		Driver off
0x1748		AB9	mcasp5_fsx	mcasp5_fsr	spi4_d1	uart9_txd	i2c5_scl		vout2_d21		vin4a_d21	vin5a_d10			pr2_pru1_gpi2	pr2_pru1_gpo2		Driver off
0x174C		AB3	mcasp5_axr0		spi4_d0	uart9_ctsn	uart3_rxd		vout2_d22		vin4a_d22	vin5a_d9		pr2_mdio_mdclk	pr2_pru1_gpi3	pr2_pru1_gpo3		Driver off
0x1750		AA4	mcasp5_axr1		spi4_cs0	uart9_rtsn	uart3_txd		vout2_d23		vin4a_d23	vin5a_d8		pr2_mdio_data	pr2_pru1_gpi4	pr2_pru1_gpo4		Driver off
0x1754	μSD clk	W6	mmc1_clk														gpio6_21	Driver off
0x1758	μSD cmd	Y6	mmc1_cmd														gpio6_22	Driver off
0x175C	μSD d0	AA6	mmc1_dat0														gpio6_23	Driver off
0x1760	μSD d1	Y4	mmc1_dat1														gpio6_24	Driver off
0x1764	μSD d2	AA5	mmc1_dat2														gpio6_25	Driver off
0x1768	μSD d3	Y3	mmc1_dat3														gpio6_26	Driver off
0x176C	μSD cd	W7	mmc1_sdcd			uart6_rxd	i2c4_sda										gpio6_27	Driver off
0x1770		Y9	mmc1_sdwp			uart6_txd	i2c4_scl										gpio6_28	Driver off
0x1774		AC5	gpio6_10	mdio_mclk	i2c3_sda		vin2b_hsync1					vin5a_clk0	ehrpwm2A	pr2_mii_mt1_clk	pr2_pru0_gpi0	pr2_pru0_gpo0	gpio6_10	Driver off
0x1778		AB4	gpio6_11	mdio_d	i2c3_scl		vin2b_vsync1					vin5a_de0	ehrpwm2B	pr2_mii1_txen	pr2_pru0_gpi1	pr2_pru0_gpo1	gpio6_11	Driver off
0x177C	P8.21	AD4	mmc3_clk				vin2b_d7					vin5a_d7	ehrpwm2_tripzone_input	pr2_mii1_txd3	pr2_pru0_gpi2	pr2_pru0_gpo2	gpio6_29	Driver off
0x1780	P8.20	AC4	mmc3_cmd	spi3_sclk			vin2b_d6					vin5a_d6	eCAP2_in_PWM2_out	pr2_mii1_txd2	pr2_pru0_gpi3	pr2_pru0_gpo3	gpio6_30	Driver off
0x1784	P8.25	AC7	mmc3_dat0	spi3_d1	uart5_rxd		vin2b_d5					vin5a_d5	eQEP3A_in	pr2_mii1_txd1	pr2_pru0_gpi4	pr2_pru0_gpo4	gpio6_31	Driver off
0x1788	P8.24	AC6	mmc3_dat1	spi3_d0	uart5_txd		vin2b_d4					vin5a_d4	eQEP3B_in	pr2_mii1_txd0	pr2_pru0_gpi5	pr2_pru0_gpo5	gpio7_0	Driver off
0x178C	P8.05	AC9	mmc3_dat2	spi3_cs0	uart5_ctsn		vin2b_d3					vin5a_d3	eQEP3_index	pr2_mii_mr1_clk	pr2_pru0_gpi6	pr2_pru0_gpo6	gpio7_1	Driver off
0x1790	P8.06	AC3	mmc3_dat3	spi3_cs1	uart5_rtsn		vin2b_d2					vin5a_d2	eQEP3_strobe	pr2_mii1_rxdv	pr2_pru0_gpi7	pr2_pru0_gpo7	gpio7_2	Driver off
0x1794	P8.23	AC8	mmc3_dat4	spi4_sclk	uart10_rxd		vin2b_d1					vin5a_d1	ehrpwm3A	pr2_mii1_rxd3	pr2_pru0_gpi8	pr2_pru0_gpo8	gpio1_22	Driver off
0x1798	P8.22	AD6	mmc3_dat5	spi4_d1	uart10_txd		vin2b_d0					vin5a_d0	ehrpwm3B	pr2_mii1_rxd2	pr2_pru0_gpi9	pr2_pru0_gpo9	gpio1_23	Driver off
0x179C	P8.03	AB8	mmc3_dat6	spi4_d0	uart10_ctsn		vin2b_de1					vin5a_hsync0	ehrpwm3_tripzone_input	pr2_mii1_rxd1	pr2_pru0_gpi10	pr2_pru0_gpo10	gpio1_24	Driver off
0x17A0	P8.04	AB5	mmc3_dat7	spi4_cs0	uart10_rtsn		vin2b_clk1					vin5a_vsync0	eCAP3_in_PWM3_out	pr2_mii1_rxd0	pr2_pru0_gpi11	pr2_pru0_gpo11	gpio1_25	Driver off
0x17A4		A25	spi1_sclk														gpio7_7	Driver off
0x17A8		F16	spi1_d1														gpio7_8	Driver off
0x17AC		B25	spi1_d0														gpio7_9	Driver off
0x17B0		A24	spi1_cs0														gpio7_10	Driver off
0x17B4	P9.23	A22	spi1_cs1		sata1_led	spi2_cs1											gpio7_11	Driver off
0x17B8	hdmi ddc hpd	B21	spi1_cs2	uart4_rxd	mmc3_sdcd	spi2_cs2	dcan2_tx	mdio_mclk	hdmi1_hpd								gpio7_12	Driver off
0x17BC	hdmi ddc cec	B20	spi1_cs3	uart4_txd	mmc3_sdwp	spi2_cs3	dcan2_rx	mdio_d	hdmi1_cec								gpio7_13	Driver off
0x17C0	P9.22b	A26	spi2_sclk	uart3_rxd													gpio7_14	Driver off
0x17C4	P9.21b	B22	spi2_d1	uart3_txd													gpio7_15	Driver off
0x17C8	P9.18a	G17	spi2_d0	uart3_ctsn	uart5_rxd												gpio7_16	Driver off
0x17CC	P9.17a	B24	spi2_cs0	uart3_rtsn	uart5_txd												gpio7_17	Driver off
0x17D0		G20	dcan1_tx		uart8_rxd	mmc2_sdcd			hdmi1_hpd								gpio1_14	Driver off
0x17D4		G19	dcan1_rx		uart8_txd	mmc2_sdwp	sata1_led		hdmi1_cec								gpio1_15	Driver off
0x17D8
0x17DC
0x17E0	console in	B27	uart1_rxd			mmc4_sdcd											gpio7_22	Driver off
0x17E4	console out	C26	uart1_txd			mmc4_sdwp											gpio7_23	Driver off
0x17E8	wifi sdio clk	E25	uart1_ctsn		uart9_rxd	mmc4_clk											gpio7_24	Driver off
0x17EC	wifi sdio cmd	C27	uart1_rtsn		uart9_txd	mmc4_cmd											gpio7_25	Driver off
0x17F0	wifi sdio d0	D28		uart3_ctsn	uart3_rctx	mmc4_dat0	uart2_rxd	uart1_dcdn									gpio7_26	Driver off
0x17F4	wifi sdio d1	D26	uart2_txd	uart3_rtsn	uart3_sd	mmc4_dat1	uart2_txd	uart1_dsrn									gpio7_27	Driver off
0x17F8	wifi sdio d2	D27	uart2_ctsn		uart3_rxd	mmc4_dat2	uart10_rxd	uart1_dtrn									gpio1_16	Driver off
0x17FC	wifi sdio d3	C28	uart2_rtsn	uart3_txd	uart3_irtx	mmc4_dat3	uart10_txd	uart1_rin									gpio1_17	Driver off
0x1800	local i²c sda	C21	i2c1_sda															
0x1804	local i²c scl	C20	i2c1_scl															
0x1808	hdmi ddc scl	C25	i2c2_sda	hdmi1_ddc_scl														Driver off
0x180C	hdmi ddc sda	F17	i2c2_scl	hdmi1_ddc_sda														Driver off
0x1810
0x1814
0x1818		AD17	Wakeup0	dcan1_rx													gpio1_0	Driver off
0x181C		AC17	Wakeup1	dcan2_rx													gpio1_1	Driver off
0x1820		AB16	Wakeup2	sys_nirq2													gpio1_2	Driver off
0x1824		AC16	Wakeup3	sys_nirq1													gpio1_3	Driver off
0x1828		Y11	on_off															
0x182C	rtc por reset	AB17	rtc_porz															
0x1830	jtag tms	F18	tms															
0x1834	jtag tdi	D23	tdi														gpio8_27	
0x1838	jtag tdo	F19	tdo														gpio8_28	
0x183C	jtag tclk	E20	tclk															
0x1840	jtag trst	D20	trstn															
0x1844	jtag rtck	E18	rtck														gpio8_29	
0x1848	jtag emu0	G21	emu0														gpio8_30	
0x184C	jtag emu1	D24	emu1														gpio8_31	
0x185C	reset	E23	resetn															
0x1860		D21	nmin_dsp															
0x1864	reset out	F23	rstoutn															

__AM335X__
# mode 0-7, label, ball
gpmc d0			mmc 1 d0		-			-			-			-			-			gpio 1.00		P8.25 / eMMC d0	U7
gpmc d1			mmc 1 d1		-			-			-			-			-			gpio 1.01		P8.24 / eMMC d1	V7
gpmc d2			mmc 1 d2		-			-			-			-			-			gpio 1.02		P8.05 / eMMC d2	R8
gpmc d3			mmc 1 d3		-			-			-			-			-			gpio 1.03		P8.06 / eMMC d3	T8
gpmc d4			mmc 1 d4		-			-			-			-			-			gpio 1.04		P8.23 / eMMC d4	U8
gpmc d5			mmc 1 d5		-			-			-			-			-			gpio 1.05		P8.22 / eMMC d5	V8
gpmc d6			mmc 1 d6		-			-			-			-			-			gpio 1.06		P8.03 / eMMC d6	R9
gpmc d7			mmc 1 d7		-			-			-			-			-			gpio 1.07		P8.04 / eMMC d7	T9
gpmc d8			lcd d23			mmc 1 d0		mmc 2 d4		pwm 2 out A		pru mii 0 txclk		-			gpio 0.22		P8.19	U10
gpmc d9			lcd d22			mmc 1 d1		mmc 2 d5		pwm 2 out B		pru mii 0 col		-			gpio 0.23		P8.13	T10
gpmc d10		lcd d21			mmc 1 d2		mmc 2 d6		pwm 2 tripzone		pru mii 0 txen		-			gpio 0.26		P8.14	T11
gpmc d11		lcd d20			mmc 1 d3		mmc 2 d7		pwm sync out		pru mii 0 txd3		-			gpio 0.27		P8.17	U12
gpmc d12		lcd d19			mmc 1 d4		mmc 2 d0		qep 2 in A		pru mii 0 txd2		pru 0 out 14		gpio 1.12		P8.12	T12
gpmc d13		lcd d18			mmc 1 d5		mmc 2 d1		qep 2 in B		pru mii 0 txd1		pru 0 out 15		gpio 1.13		P8.11	R12
gpmc d14		lcd d17			mmc 1 d6		mmc 2 d2		qep 2 index		pru mii 0 txd0		pru 0 in 14		gpio 1.14		P8.16	V13
gpmc d15		lcd d16			mmc 1 d7		mmc 2 d3		qep 2 strobe		pru eCAP		pru 0 in 15		gpio 1.15		P8.15	U13
gpmc a0			mii 1 txen		rgmii 1 txctl		rmii 1 txen		gpmc a16		pru mii 1 txclk		pwm 1 tripzone		gpio 1.16		P9.15	R13
gpmc a1			mii 1 rxdv		rgmii 1 rxctl		mmc 2 d0		gpmc a17		pru mii 1 txd3		pwm sync out		gpio 1.17		P9.23	V14
gpmc a2			mii 1 txd3		rgmii 1 txd3		mmc 2 d1		gpmc a18		pru mii 1 txd2		pwm 1 out A		gpio 1.18		P9.14	U14
gpmc a3			mii 1 txd2		rgmii 1 txd2		mmc 2 d2		gpmc a19		pru mii 1 txd1		pwm 1 out B		gpio 1.19		P9.16	T14
gpmc a4			mii 1 txd1		rgmii 1 txd1		rmii 1 txd1		gpmc a20		pru mii 1 txd0		qep 1 in A		gpio 1.20		eMMC reset	R14
gpmc a5			mii 1 txd0		rgmii 1 txd0		rmii 1 txd0		gpmc a21		pru mii 1 rxd3		qep 1 in B		gpio 1.21		user led 0	V15
gpmc a6			mii 1 txclk		rgmii 1 txclk		mmc 2 d4		gpmc a22		pru mii 1 rxd2		qep 1 index		gpio 1.22		user led 1	U15
gpmc a7			mii 1 rxclk		rgmii 1 rxclk		mmc 2 d5		gpmc a23		pru mii 1 rxd1		qep 1 strobe		gpio 1.23		user led 2	T15
gpmc a8			mii 1 rxd3		rgmii 1 rxd3		mmc 2 d6		gpmc a24		pru mii 1 rxd0		asp 0 tx clk		gpio 1.24		user led 3	V16
gpmc a9			mii 1 rxd2		rgmii 1 rxd2		mmc 2 d7		gpmc a25		pru mii 1 rxclk		asp 0 tx fs		gpio 1.25		hdmi irq	U16
gpmc a10		mii 1 rxd1		rgmii 1 rxd1		rmii 1 rxd1		gpmc a26		pru mii 1 rxdv		asp 0 data 0		gpio 1.26		usb A vbus overcurrent	T16
gpmc a11		mii 1 rxd0		rgmii 1 rxd0		rmii 1 rxd0		gpmc a27		pru mii 1 rxer		asp 0 data 1		gpio 1.27		audio osc enable	V17
gpmc wait 0		mii 1 crs		gpmc cs 4		rmii 1 crs/rxdv		mmc 1 cd		pru mii 1 col		uart 4 rxd		gpio 0.30		P9.11	T17
gpmc wp			mii 1 rxer		gpmc cs 5		rmii 1 rxer		mmc 2 cd		pru mii 1 txen		uart 4 txd		gpio 0.31		P9.13	U17
gpmc be1		mii 1 col		gpmc cs 6		mmc 2 d3		gpmc dir		pru mii 1 rxlink	asp 0 rx clk		gpio 1.28		P9.12	U18
gpmc cs 0		-			-			-			-			-			-			gpio 1.29		P8.26	V6
gpmc cs 1		gpmc clk		mmc 1 clk		pru edio in 6		pru edio out 6		pru 1 out 12		pru 1 in 12		gpio 1.30		P8.21 / eMMC clk	U9
gpmc cs 2		gpmc be1		mmc 1 cmd		pru edio in 7		pru edio out 7		pru 1 out 13		pru 1 in 13		gpio 1.31		P8.20 / eMMC cmd	V9
gpmc cs 3		gpmc a3			rmii 1 crs/rxdv		mmc 2 cmd		pru mii 0 crs		pru mdio data		jtag emu 4		gpio 2.00		P9.15	T13
gpmc clk		lcd mclk		gpmc wait 1		mmc 2 clk		pru mii 1 crs		pru mdio clock		asp 0 rx fs		gpio 2.01		P8.18	V12
gpmc adv/ale		-			timer 4			-			-			-			-			gpio 2.02		P8.07	R7
gpmc oe/re		-			timer 7			-			-			-			-			gpio 2.03		P8.08	T7
gpmc we			-			timer 6			-			-			-			-			gpio 2.04		P8.10	U6
gpmc be0/cle		-			timer 5			-			-			-			-			gpio 2.05		P8.09	T6
lcd d0			gpmc a0			pru mii 0 txclk		pwm 2 out A		-			pru 1 out 0		pru 1 in 0		gpio 2.06		P8.45 / hdmi / sysboot 0	R1
lcd d1			gpmc a1			pru mii 0 txen		pwm 2 out B		-			pru 1 out 1		pru 1 in 1		gpio 2.07		P8.46 / hdmi / sysboot 1	R2
lcd d2			gpmc a2			pru mii 0 txd3		pwm 2 tripzone		-			pru 1 out 2		pru 1 in 2		gpio 2.08		P8.43 / hdmi / sysboot 2	R3
lcd d3			gpmc a3			pru mii 0 txd2		pwm sync out		-			pru 1 out 3		pru 1 in 3		gpio 2.09		P8.44 / hdmi / sysboot 3	R4
lcd d4			gpmc a4			pru mii 0 txd1		qep 2 in A		-			pru 1 out 4		pru 1 in 4		gpio 2.10		P8.41 / hdmi / sysboot 4	T1
lcd d5			gpmc a5			pru mii 0 txd0		qep 2 in B		-			pru 1 out 5		pru 1 in 5		gpio 2.11		P8.42 / hdmi / sysboot 5	T2
lcd d6			gpmc a6			pru edio in 6		qep 2 index		pru edio out 6		pru 1 out 6		pru 1 in 6		gpio 2.12		P8.39 / hdmi / sysboot 6	T3
lcd d7			gpmc a7			pru edio in 7		qep 2 strobe		pru edio out 7		pru 1 out 7		pru 1 in 7		gpio 2.13		P8.40 / hdmi / sysboot 7	T4
lcd d8			gpmc a12		pwm 1 tripzone		asp 0 tx clk		uart 5 txd		pru mii 0 rxd3		uart 2 cts		gpio 2.14		P8.37 / hdmi / sysboot 8	U1
lcd d9			gpmc a13		pwm sync out		asp 0 tx fs		uart 5 rxd		pru mii 0 rxd2		uart 2 rts		gpio 2.15		P8.38 / hdmi / sysboot 9	U2
lcd d10			gpmc a14		pwm 1 out A		asp 0 data 0		-			pru mii 0 rxd1		uart 3 cts		gpio 2.16		P8.36 / hdmi / sysboot 10	U3
lcd d11			gpmc a15		pwm 1 out B		asp 0 rx hclk		asp 0 data 2		pru mii 0 rxd0		uart 3 rts		gpio 2.17		P8.34 / hdmi / sysboot 11	U4
lcd d12			gpmc a16		qep 1 in A		asp 0 rx clk		asp 0 data 2		pru mii 0 rxlink	uart 4 cts		gpio 0.08		P8.35 / hdmi / sysboot 12	V2
lcd d13			gpmc a17		qep 1 in B		asp 0 rx fs		asp 0 data 3		pru mii 0 rxer		uart 4 rts		gpio 0.09		P8.33 / hdmi / sysboot 13	V3
lcd d14			gpmc a18		qep 1 index		asp 0 data 1		uart 5 rxd		pru mii 0 rxclk		uart 5 cts		gpio 0.10		P8.31 / hdmi / sysboot 14	V4
lcd d15			gpmc a19		qep 1 strobe		asp 0 tx hclk		asp 0 data 3		pru mii 0 rxdv		uart 5 rts		gpio 0.11		P8.32 / hdmi / sysboot 15	T5
lcd vsync		gpmc a8			gpmc a1			pru edio in 2		pru edio out 2		pru 1 out 8		pru 1 in 8		gpio 2.22		P8.27 / hdmi	U5
lcd hsync		gpmc a9			gpmc a2			pru edio in 3		pru edio out 3		pru 1 out 9		pru 1 in 9		gpio 2.23		P8.29 / hdmi	R5
lcd pclk		gpmc a10		pru mii 0 crs		pru edio in 4		pru edio out 4		pru 1 out 10		pru 1 in 10		gpio 2.24		P8.28 / hdmi	V5
lcd oe/acb		gpmc a11		pru mii 1 crs		pru edio in 5		pru edio out 5		pru 1 out 11		pru 1 in 11		gpio 2.25		P8.30 / hdmi	R6
mmc 0 d3		gpmc a20		uart 4 cts		timer 5			uart 1 dcd		pru 0 out 8		pru 0 in 8		gpio 2.26		μSD d3	F17
mmc 0 d2		gpmc a21		uart 4 rts		timer 6			uart 1 dsr		pru 0 out 9		pru 0 in 9		gpio 2.27		μSD d2	F18
mmc 0 d1		gpmc a22		uart 5 cts		uart 3 rxd		uart 1 dtr		pru 0 out 10		pru 0 in 10		gpio 2.28		μSD d1	G15
mmc 0 d0		gpmc a23		uart 5 rts		uart 3 txd		uart 1 ri		pru 0 out 11		pru 0 in 11		gpio 2.29		μSD d0	G16
mmc 0 clk		gpmc a24		uart 3 cts		uart 2 rxd		can 1 tx		pru 0 out 12		pru 0 in 12		gpio 2.30		μSD clk	G17
mmc 0 cmd		gpmc a25		uart 3 rts		uart 2 txd		can 1 rx		pru 0 out 13		pru 0 in 13		gpio 2.31		μSD cmd	G18
mii 0 col		rmii 1 refclk		spi 1 clk		uart 5 rxd		asp 1 data 2		mmc 2 d3		asp 0 data 2		gpio 3.00		eth mii col	H16
mii 0 crs		rmii 0 crs/rxdv		spi 1 data 0		i²c 1 sda		asp 1 tx clk		uart 5 cts		uart 2 rxd		gpio 3.01		eth mii crs	H17
mii 0 rxer		rmii 0 rxer		spi 1 data 1		i²c 1 scl		asp 1 tx fs		uart 5 rts		uart 2 txd		gpio 3.02		eth mii rx err	J15
mii 0 txen		rmii 0 txen		rgmii 0 txctl		timer 4			asp 1 data 0		qep 0 index		mmc 2 cmd		gpio 3.03		eth mii tx en	J16
mii 0 rxdv		lcd mclk		rgmii 0 rxctl		uart 5 txd		asp 1 tx clk		mmc 2 d0		asp 0 rx clk		gpio 3.04		eth mii rx dv	J17
mii 0 txd3		can 0 tx		rgmii 0 txd3		uart 4 rxd		asp 1 tx fs		mmc 2 d1		asp 0 rx fs		gpio 0.16		eth mii tx d3	J18
mii 0 txd2		can 0 rx		rgmii 0 txd2		uart 4 txd		asp 1 data 0		mmc 2 d2		asp 0 tx hclk		gpio 0.17		eth mii tx d2	K15
mii 0 txd1		rmii 0 txd1		rgmii 0 txd1		asp 1 rx fs		asp 1 data 1		qep 0 in A		mmc 1 cmd		gpio 0.21		eth mii tx d1	K16
mii 0 txd0		rmii 0 txd0		rgmii 0 txd0		asp 1 data 2		asp 1 rx clk		qep 0 in B		mmc 1 clk		gpio 0.28		eth mii tx d0	K17
mii 0 txclk		uart 2 rxd		rgmii 0 txclk		mmc 0 d7		mmc 1 d0		uart 1 dcd		asp 0 tx clk		gpio 3.09		eth mii tx clk	K18
mii 0 rxclk		uart 2 txd		rgmii 0 rxclk		mmc 0 d6		mmc 1 d1		uart 1 dsr		asp 0 tx fs		gpio 3.10		eth mii rx clk	L18
mii 0 rxd3		uart 3 rxd		rgmii 0 rxd3		mmc 0 d5		mmc 1 d2		uart 1 dtr		asp 0 data 0		gpio 2.18		eth mii rx d3	L17
mii 0 rxd2		uart 3 txd		rgmii 0 rxd2		mmc 0 d4		mmc 1 d3		uart 1 ri		asp 0 data 1		gpio 2.19		eth mii rx d2	L16
mii 0 rxd1		rmii 0 rxd1		rgmii 0 rxd1		asp 1 data 3		asp 1 rx fs		qep 0 strobe		mmc 2 clk		gpio 2.20		eth mii rx d1	L15
mii 0 rxd0		rmii 0 rxd0		rgmii 0 rxd0		asp 1 tx hclk		asp 1 rx hclk		asp 1 rx clk		asp 0 data 3		gpio 2.21		eth mii rx d0	M16
rmii 0 refclk		dma event 2		spi 1 cs 0		uart 5 txd		asp 1 data 3		mmc 0 pow		asp 1 tx hclk		gpio 0.29		-eth unused	H18
mdio data		timer 6			uart 5 rxd		uart 3 cts		mmc 0 cd		mmc 1 cmd		mmc 2 cmd		gpio 0.00		eth mdio	M17
mdio clk		timer 5			uart 5 txd		uart 3 rts		mmc 0 wp		mmc 1 clk		mmc 2 clk		gpio 0.01		eth mdc	M18
spi 0 clk		uart 2 rxd		i²c 2 sda		pwm 0 out A		uart pru cts		pru edio sof		jtag emu 2		gpio 0.02		P9.22 / spi boot clk	A17
spi 0 data 0		uart 2 txd		i²c 2 scl		pwm 0 out B		uart pru rts		pru edio latch		jtag emu 3		gpio 0.03		P9.21 / spi boot in	B17
spi 0 data 1		mmc 1 wp		i²c 1 sda		pwm 0 tripzone		uart pru rxd		pru edio in 0		pru edio out 0		gpio 0.04		P9.18 / spi boot out	B16
spi 0 cs 0		mmc 2 wp		i²c 1 scl		pwm sync in		uart pru txd		pru edio in 1		pru edio out 1		gpio 0.05		P9.17 / spi boot cs	A16
spi 0 cs 1		uart 3 rxd		eCAP 1			mmc 0 pow		dma event 2		mmc 0 cd		jtag emu 4		gpio 0.06		μSD cd / jtag emu4	C15
eCAP 0			uart 3 txd		spi 1 cs 1		pru eCAP		spi 1 clk		mmc 0 wp		dma event 2		gpio 0.07		P9.42a	C18
uart 0 cts		uart 4 rxd		can 1 tx		i²c 1 sda		spi 1 data 0		timer 7			pru edc sync 0 out	gpio 1.08		-×	E18
uart 0 rts		uart 4 txd		can 1 rx		i²c 1 scl		spi 1 data 1		spi 1 cs 0		pru edc sync 1 out	gpio 1.09		-TP9	E17
uart 0 rxd		spi 1 cs 0		can 0 tx		i²c 2 sda		eCAP 2			pru 1 out 14		pru 1 in 14		gpio 1.10		console in	E15
uart 0 txd		spi 1 cs 1		can 0 rx		i²c 2 scl		eCAP 1			pru 1 out 15		pru 1 in 15		gpio 1.11		console out	E16
uart 1 cts		timer 6			can 0 tx		i²c 2 sda		spi 1 cs 0		uart pru cts		pru edc latch 0		gpio 0.12		P9.20 / cape i²c sda	D18
uart 1 rts		timer 5			can 0 rx		i²c 2 scl		spi 1 cs 1		uart pru rts		pru edc latch 1		gpio 0.13		P9.19 / cape i²c scl	D17
uart 1 rxd		mmc 1 wp		can 1 tx		i²c 1 sda		-			uart pru rxd		pru 1 in 16		gpio 0.14		P9.26	D16
uart 1 txd		mmc 2 wp		can 1 rx		i²c 1 scl		-			uart pru txd		pru 0 in 16		gpio 0.15		P9.24	D15
i²c 0 sda		timer 4			uart 2 cts		eCAP 2			-			-			-			gpio 3.05		local i²c sda	C17
i²c 0 scl		timer 7			uart 2 rts		eCAP 1			-			-			-			gpio 3.06		local i²c scl	C16
asp 0 tx clk		pwm 0 out A		-			spi 1 clk		mmc 0 cd		pru 0 out 0		pru 0 in 0		gpio 3.14		P9.31 / hdmi audio clk	A13
asp 0 tx fs		pwm 0 out B		-			spi 1 data 0		mmc 1 cd		pru 0 out 1		pru 0 in 1		gpio 3.15		P9.29 / hdmi audio fs	B13
asp 0 data 0		pwm 0 tripzone		-			spi 1 data 1		mmc 2 cd		pru 0 out 2		pru 0 in 2		gpio 3.16		P9.30	D12
asp 0 rx hclk		pwm sync in		asp 0 data 2		spi 1 cs 0		eCAP 2			pru 0 out 3		pru 0 in 3		gpio 3.17		P9.28 / hdmi audio data	C12
asp 0 rx clk		qep 0 in A		asp 0 data 2		asp 1 tx clk		mmc 0 wp		pru 0 out 4		pru 0 in 4		gpio 3.18		P9.42b	B12
asp 0 rx fs		qep 0 in B		asp 0 data 3		asp 1 tx fs		jtag emu 2		pru 0 out 5		pru 0 in 5		gpio 3.19		P9.27	C13
asp 0 data 1		qep 0 index		-			asp 1 data 0		jtag emu 3		pru 0 out 6		pru 0 in 6		gpio 3.20		P9.41	D13
asp 0 tx hclk		qep 0 strobe		asp 0 data 3		asp 1 data 1		jtag emu 4		pru 0 out 7		pru 0 in 7		gpio 3.21		P9.25 / audio osc	A14
dma event 0		-			timer 4			clkout 0		spi 1 cs 1		pru 1 in 16		jtag emu 2		gpio 0.19		hdmi cec / jtag emu2	A15
dma event 1		-			timer clkin		clkout 1		timer 7			pru 0 in 16		jtag emu 3		gpio 0.20		P9.41 / jtag emu3	D14
reset in/out		-			-			-			-			-			-			-			-reset	A10
por			-			-			-			-			-			-			-			-pmic	B15
mpu irq			-			-			-			-			-			-			-			pmic irq	B18
osc 0 in		-			-			-			-			-			-			-			-xtal
osc 0 out		-			-			-			-			-			-			-			-xtal
osc 0 gnd		-			-			-			-			-			-			-			-xtal
jtag tms		-			-			-			-			-			-			-			-jtag	C11
jtag tdi		-			-			-			-			-			-			-			-jtag	B11
jtag tdo		-			-			-			-			-			-			-			-jtag	A11
jtag tck		-			-			-			-			-			-			-			-jtag	A12
jtag trst		-			-			-			-			-			-			-			-jtag	B10
emu 0			-			-			-			-			-			-			gpio 3.07		jtag emu0	C14
emu 1			-			-			-			-			-			-			gpio 3.08		jtag emu1	B14
osc 1 (rtc) in		-			-			-			-			-			-			-			-rtc xtal
osc 1 (rtc) out		-			-			-			-			-			-			-			-rtc xtal
rtc gnd			-			-			-			-			-			-			-			-rtc xtal
rtc por			-			-			-			-			-			-			-			-pmic	B5
power en		-			-			-			-			-			-			-			-pmic
wakeup			-			-			-			-			-			-			-			-pmic
rtc ldo en		-			-			-			-			-			-			-			-pmic
usb 0 d−		-			-			-			-			-			-			-			-usb B
usb 0 d+		-			-			-			-			-			-			-			-usb B
usb 0 charge en		-			-			-			-			-			-			-			-x
usb 0 id		-			-			-			-			-			-			-			-usb B id
usb 0 vbus in		-			-			-			-			-			-			-			-usb B vbus
usb 0 vbus out en	-			-			-			-			-			-			gpio 0.18		-×	F16
usb 1 d−		-			-			-			-			-			-			-			-usb A
usb 1 d+		-			-			-			-			-			-			-			-usb A
usb 1 charge en		-			-			-			-			-			-			-			-×
usb 1 id		-			-			-			-			-			-			-			-gnd
usb 1 vbus in		-			-			-			-			-			-			-			-usb A vbus
usb 1 vbus out en	-			-			-			-			-			-			gpio 3.13		usb A vbus en	F15
# vim: ts=8:sts=0:noet:nowrap
