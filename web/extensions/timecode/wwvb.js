// Copyright (c) 2017 John Seamons, ZL/KF6VO

var wwvb_ph_sync = [ 0,0,0,1,1,1,0,1,1,0,1,0,0,0 ];

function wwvb_sync()
{
	var l = tc_data.length;
	var sync = 1;
	for (i = 0; i < 14; i++) {
		if (tc_data[l-(14-i)] != wwvb_ph_sync[i]) {
			sync = 0;
			break;
		}
	}
	if (!sync) {
			sync = 2;
			for (i = 0; i < 14; i++) {
				if (tc_data[l-(14-i)] != (wwvb_ph_sync[i] ^ 1)) {
					sync = 0;
					break;
				}
			}
	}
	
	return sync;
}

function wwvb(d)
{
	var i;
	tc_trig++; if (tc_trig >= 100) tc_trig = 0;
	
   if (d == tc_cur) {
   	tc_cnt = 0;
   } else {
   	tc_cnt++;
   	if (tc_cnt > TC_NOISE_THRESHOLD) {
   		tc_cur = d;
   		tc_cnt = 0;
   		if (tc_cur) {
   			tc_phdata ^= 1;		// flip phase data on 0 -> 1 transition of phase reversal
   		}
   	}
   }

	if (tc_phdata) {
		tc_width++;
	}
	
	// if we need it, measure width on falling edge tc_phdata
	if (tc_sample_point == -1 && tc_phdata_last == 1 && tc_phdata == 0) {		
		tc_dmsg(tc_width.toString() +' ');
		if (tc_width > 50 && tc_width < 110) {
			tc_sample_point = tc_trig - Math.round(tc_width/2);
			if (tc_sample_point < 0) tc_sample_point += 100;
			tc_dmsg('T='+ tc_sample_point.toString() +' ');
		}
		tc_width = 0;
	}
	
	if (tc_sample_point == tc_trig) {
		if (tc_state == tc_st.ACQ_SYNC) {
			tc_dmsg(tc_phdata.toString());
			tc_data.push(tc_phdata);
			
			if (tc_data.length >= 14) {
				tc_inverted = 0;
				var sync = wwvb_sync();
				if (sync == 2) tc_inverted = 1;
				if (sync) {
					tc_stat('magenta', 'Found sync');
					tc_dmsg(' SYNC'+ (tc_inverted? '-I':'') +'<br>00011101101000 ');
					tc_dcnt = 13;
					tc_state = tc_st.ACQ_DATA;
					tc_data = [];
					tc_min = 0;
				}
			}

			tc_recv++;
			if (tc_recv > (65 + Math.round(Math.random()*20))) {		// no sync, pick another sample point
				tc_dmsg(' RESTART<br>');
				tc_sample_point = -1;
				tc_width = 0;
				tc_recv = 0;
			}
		} else
		
		if (tc_state == tc_st.ACQ_DATA) {
			var data = tc_phdata ^ tc_inverted;
			tc_data.push(data);
			tc_dmsg(data.toString());
			if (tc_dcnt == 12 ||
					tc_dcnt == 17 ||
					tc_dcnt == 28 ||
					tc_dcnt == 29 ||
					tc_dcnt == 38 ||
					tc_dcnt == 39 ||
					tc_dcnt == 46 ||
					tc_dcnt == 52)
				tc_dmsg(' ');
			
			if (tc_dcnt == 18 || (tc_dcnt >= 20 && tc_dcnt <= 46 && tc_dcnt != 29 && tc_dcnt != 39)) {
				tc_min = (tc_min << 1) + data;
				//console.log(tc_dcnt +': '+ data +' '+ tc_min);
			}

			if (tc_dcnt == 19) tc_time0_copy = data;
			if (tc_dcnt == 46) tc_time0 = data;
			
			if (tc_data.length >= 14 && tc_dcnt == 12) {
				var sync = wwvb_sync();
				if (!sync) {
					tc_dmsg('RESYNC<br>');
					tc_state = tc_st.ACQ_SYNC;
					tc_sample_point = -1;
					tc_width = 0;
					tc_recv = 0;
				}
				if ((sync == 1 && tc_inverted) || (sync == 2 && !tc_inverted)) {
					tc_dmsg(' PHASE INVERSION<br>00011101101000 ');
					tc_inverted ^= 1;
				}
			}
			
			// NB: This only works for the 21st century.
			if (tc_dcnt == 58) {
				if (tc_min && tc_time0_copy != tc_time0) {
					tc_dmsg(' TIME BIT-0 MISMATCH');
				} else
				if (tc_min) {
					tc_dmsg(' '+ tc_min.toString());
					console.log('start '+ tc_min);
					var m = tc_min - (tc_MpY + tc_MpD);	// 2000 is LY
					m++;		// minute increments just as we print decode
					console.log('2000: '+ m);
					var yr = 2001;
					var LY;
					while (true) {
						LY = ((yr & 3) == 0);
						var MpY = tc_MpY + (LY? tc_MpD : 0);
						if (m > MpY) {
							m -= MpY;
							console.log(yr +': '+ MpY +' '+ m);
							yr++;
						} else {
							break;
						}
					}
					console.log('yr done: '+ m);
					
					var mo = 0;
					while (true) {
						var days = tc_dim[mo];
						if (mo == 1 && LY) days++;
						var MpM = tc_MpD * days;
						console.log(tc_mo[mo] +': LY'+ LY +' '+ days);
						if (m > MpM) {
							m -= MpM;
							console.log(tc_mo[mo] +': '+ MpM +' '+ m);
							mo++;
						} else {
							break;
						}
					}
					console.log('mo done: '+ m +' '+ (m/24));
					var day = Math.floor(m / tc_MpD);
					m -= day * tc_MpD;
					var hour = Math.floor(m / 60);
					m -= hour * 60;
					tc_dmsg(' '+ (day+1) +' '+ tc_mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					tc_stat('lime', 'Time decoded: '+ (day+1) +' '+ tc_mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					
					if (m == 10 || m == 40) tc_sixmin_mode = 1;
				}
				
				tc_dmsg('<br>');
				tc_data = [];
				tc_min = 0;
			}
			
			tc_dcnt++;
			if (tc_dcnt == 60) tc_dcnt = 0;

			if (tc_dcnt == 0 && tc_sixmin_mode) {
				tc_sixmin_mode = 0;
				tc_state = tc_st.ACQ_SIXMIN;
			}
		} else

		if (tc_state == tc_st.ACQ_SIXMIN) {
			if (tc_dcnt == 0) tc_dmsg('127b: ');
			if (tc_dcnt == 127) tc_dmsg('<br>106b: ');
			if (tc_dcnt == 233) tc_dmsg('<br>127b: ');

			var data = tc_phdata ^ tc_inverted;
			tc_dmsg(data.toString());

			tc_dcnt++;
			if (tc_dcnt == 360) {
				tc_dmsg('<br>');
				tc_dcnt = 0;
				tc_state = tc_st.ACQ_DATA;
			}
		}
	}

	tc_phdata_last = tc_phdata;
}
