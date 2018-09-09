/*

from: github.com/audioplastic/ooura

Copyright (c) 2017 Nick Clark

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Copyright(C) 1996-1998 Takuya OOURA

*/

// NB: 'const', 'let', '=>' and arg defaulting not supported by older browsers!

function assert(cond, msg)
{
   if (!cond) {
      kiwi_trace();
      alert('assert failed '+ (msg? msg:''));
   }
}

var ooura32 = {
	DIRECTION_FORWARDS: +1,
	DIRECTION_BACKWARDS: -1
};

//class Ooura {
	ooura32.FFT = function (size, info) {
		assert(ooura32.isPowerOf2(size));
	   if (!info) info = {type: 'real', radix: 4};

      this_ = {};
		this_.real = (info.type === 'real');
		if (!this_.real) {
			assert(info.type === 'complex'); // Sanity
		}

		this_.size = size;
		this_.ip = new Int16Array(2 + Math.round(Math.sqrt(size)));
		this_.w = new Float32Array(size / 2);
		this_.internal = new Float32Array(size);

		ooura32.init_makewt(size / 4, this_.ip.buffer, this_.w.buffer);

		// Perform additional modification if real
		if (this_.real) {
			ooura32.init_makect(size / 4, this_.ip.buffer, this_.w.buffer, size / 4);
			this_.fft = ooura32.fftReal;
			this_.ifft = ooura32.ifftReal;
			this_.fftInPlace = ooura32.fftInPlaceReal;
			this_.ifftInPlace = ooura32.ifftInPlaceReal;
		} else {
			this_.fft = ooura32.fftComplex;
			this_.ifft = ooura32.ifftComplex;
			this_.fftInPlace = ooura32.fftInPlaceComplex;
			this_.ifftInPlace = ooura32.ifftInPlaceComplex;
		}
		
		return this_;
	};

	// Returns complex vector size given one dimensional scalar size
	/* static */ ooura32.vectorSize = function (scalarSize) {
		assert(ooura32.isPowerOf2(scalarSize));
		return (scalarSize / 2) + 1;
	};

	// Inverse fucntion of vector size
	/* static */ ooura32.scalarSize = function (vectorSize) {
		/* const */ var result = (vectorSize - 1) * 2;
		assert(ooura32.isPowerOf2(result));
		return result;
	};

	/* static */ ooura32.isPowerOf2 = function (n) {
		if (typeof n !== 'number') {
			return false;
		}
		return n && (n & (n - 1)) === 0;
	};

	ooura32.getScalarSize = function (this_) {
		return this_.size;
	};

	ooura32.getVectorSize = function (this_) {
		return ooura32.vectorSize(this_.size);
	}

	// Helper factory functions returning correct array and data size for a
	// given fft setup;
	ooura32.scalarArrayFactory = function (this_) {
		return new Float32Array(ooura32.getScalarSize(this_));
	};

	ooura32.vectorArrayFactory = function (this_) {
		return new Float32Array(ooura32.getVectorSize(this_));
	};

	// Functions below here should be called via their aliases defined in the ctor
	ooura32.fftReal = function (this_, dataBuffer, reBuffer, imBuffer) {
		/* const */ var data = new Float32Array(dataBuffer);
		this_.internal.set(data);

		ooura32.trans_rdft(this_.size, ooura32.DIRECTION_FORWARDS, this_.internal.buffer, this_.ip.buffer, this_.w.buffer);

		/* const */ var im = new Float32Array(imBuffer);
		/* const */ var re = new Float32Array(reBuffer);

		// De-interleave data
		/* let */ var nn = 0;
		/* let */ var mm = 0;
		while (nn !== this_.size) {
			re[nn] = this_.internal[mm++];
			im[nn++] = -this_.internal[mm++];
		}

		// Post cleanup
		re[this_.size / 2] = -im[0];
		im[0] = 0.0;
		im[this_.size / 2] = 0.0;
	};

	ooura32.ifftReal = function (this_, dataBuffer, reBuffer, imBuffer) {
		/* const */ var im = new Float32Array(imBuffer);
		/* const */ var re = new Float32Array(reBuffer);

		// Pack complex into buffer
		/* let */ var nn = 0;
		/* let */ var mm = 0;
		while (nn !== this_.size) {
			this_.internal[mm++] = re[nn];
			this_.internal[mm++] = -im[nn++];
		}
		this_.internal[1] = re[this_.size / 2];

		ooura32.trans_rdft(this_.size, ooura32.DIRECTION_BACKWARDS, this_.internal.buffer, this_.ip.buffer, this_.w.buffer);

		/* const */ var data = new Float32Array(dataBuffer);
		//data.set(this_.internal.map(x => x * 2 / this_.size));
		console.log("older js doesn't support '=>'");
	};

	ooura32.xfftComplex = function (this_, direction, reIpBuffer, imIpBuffer, reOpBuffer, imOpBuffer) {
		/* const */ var reIp = new Float32Array(reIpBuffer);
		/* const */ var imIp = new Float32Array(imIpBuffer);
		/* const */ var reOp = new Float32Array(reOpBuffer);
		/* const */ var imOp = new Float32Array(imOpBuffer);

		// Pack complex input into buffer
		/* let */ var nn = 0;
		/* let */ var mm = 0;
		while (nn !== this_.size) {
			this_.internal[mm++] = reIp[nn];
			this_.internal[mm++] = -imIp[nn++];
		}

		ooura32.trans_cdft(this_.size, direction, this_.internal.buffer, this_.ip.buffer, this_.w.buffer);

		// De-interleave data into output
		nn = 0;
		mm = 0;
		while (nn !== this_.size) {
			reOp[nn] = this_.internal[mm++];
			imOp[nn++] = -this_.internal[mm++];
		}
	};

	ooura32.fftComplex = function (this_, reIpBuffer, imIpBuffer, reOpBuffer, imOpBuffer) {
		ooura32.xfftComplex(this_, ooura32.DIRECTION_FORWARDS, reIpBuffer, imIpBuffer, reOpBuffer, imOpBuffer);
	};

	ooura32.ifftComplex = function (this_, reIpBuffer, imIpBuffer, reOpBuffer, imOpBuffer) {
		ooura32.xfftComplex(this_, ooura32.DIRECTION_BACKWARDS, reIpBuffer, imIpBuffer, reOpBuffer, imOpBuffer);
		/* const */ var reOp = new Float32Array(reOpBuffer);
		/* const */ var imOp = new Float32Array(imOpBuffer);
		for (/* let */ var nn = 0; nn < this_.size / 2; ++nn) {
			reOp[nn] = reOp[nn] * 2 / this_.size;
			imOp[nn] = imOp[nn] * 2 / this_.size;
		}
	};

	// Below: No-nonsense thin wrappers around the interleaved in-place data
	// representation with no scaling, for maximum throughput.
	ooura32.fftInPlaceReal = function (dataBuffer) {
		ooura32.trans_rdft(this_, this_.size, ooura32.DIRECTION_FORWARDS, dataBuffer, this_.ip.buffer, this_.w.buffer);
	};

	ooura32.fftInPlaceComplex = function (dataBuffer) {
		ooura32.trans_cdft(this_, this_.size, ooura32.DIRECTION_FORWARDS, dataBuffer, this_.ip.buffer, this_.w.buffer);
	};

	ooura32.ifftInPlaceReal = function (dataBuffer) {
		ooura32.trans_rdft(this_, this_.size, ooura32.DIRECTION_BACKWARDS, dataBuffer, this_.ip.buffer, this_.w.buffer);
	};

	ooura32.ifftInPlaceComplex = function (dataBuffer) {
		ooura32.trans_cdft(this_, this_.size, ooura32.DIRECTION_BACKWARDS, dataBuffer, this_.ip.buffer, this_.w.buffer);
	};

//}

ooura32.init_makewt = function (nw, ipBuffer, wBuffer) {
	/* let */ var nwh;
	/* let */ var delta;
	/* let */ var x;
	/* let */ var y;

	/* const */ var ip = new Int16Array(ipBuffer);
	/* const */ var w = new Float32Array(wBuffer);

	ip[0] = nw;
	ip[1] = 1;
	if (nw > 2) {
		nwh = nw >> 1;
		delta = Math.atan(1.0) / nwh;
		w[0] = 1;
		w[1] = 0;
		w[nwh] = Math.cos(delta * nwh);
		w[nwh + 1] = w[nwh];

		if (nwh > 2) {
			for (/* let */ var j = 2; j < nwh; j += 2) {
				x = Math.cos(delta * j);
				y = Math.sin(delta * j);
				w[j] = x;
				w[j + 1] = y;
				w[nw - j] = y;
				w[nw - j + 1] = x;
			}
			ooura32.child_bitrv2(nw, ip.buffer, 2, w.buffer);
		}
	}
};

ooura32.init_makect = function (nc, ipBuffer, cBuffer, cOffset) {
	/* let */ var j;
	/* let */ var nch;
	/* let */ var delta;

	/* const */ var ip = new Int16Array(ipBuffer);
	/* const */ var c = new Float32Array(cBuffer).subarray(cOffset);

	ip[1] = nc;
	if (nc > 1) {
		nch = nc >> 1;
		delta = Math.atan(1.0) / nch;
		c[0] = Math.cos(delta * nch);
		c[nch] = 0.5 * c[0];
		for (j = 1; j < nch; j++) {
			c[j] = 0.5 * Math.cos(delta * j);
			c[nc - j] = 0.5 * Math.sin(delta * j);
		}
	}
};

ooura32.trans_rdft = function (n, dir, aBuffer, ipBuffer, wBuffer) {
	/* const */ var ip = new Int16Array(ipBuffer);
	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var nw = ip[0];
	/* const */ var nc = ip[1];

	if (dir === ooura32.DIRECTION_FORWARDS) {
		if (n > 4) {
			ooura32.child_bitrv2(n, ipBuffer, 2, aBuffer);
			ooura32.child_cftfsub(n, aBuffer, wBuffer);
			ooura32.child_rftfsub(n, aBuffer, nc, wBuffer, nw);
		} else if (n === 4) {
			ooura32.child_cftfsub(n, aBuffer, wBuffer);
		}
		/* const */ var xi = a[0] - a[1];
		a[0] += a[1];
		a[1] = xi;
	} else {
		a[1] = 0.5 * (a[0] - a[1]);
		a[0] -= a[1];
		if (n > 4) {
			ooura32.child_rftbsub(n, aBuffer, nc, wBuffer, nw);
			ooura32.child_bitrv2(n, ipBuffer, 2, aBuffer);
			ooura32.child_cftbsub(n, aBuffer, wBuffer);
		} else if (n === 4) {
			ooura32.child_cftfsub(n, aBuffer, wBuffer);
		}
	}
};

ooura32.trans_cdft = function (n, dir, aBuffer, ipBuffer, wBuffer) {
	if (n > 4) {
		if (dir === ooura32.DIRECTION_FORWARDS) {
			ooura32.child_bitrv2(n, ipBuffer, 2, aBuffer);
			ooura32.child_cftfsub(n, aBuffer, wBuffer);
		} else {
			ooura32.child_bitrv2conj(n, ipBuffer, 2, aBuffer);
			ooura32.child_cftbsub(n, aBuffer, wBuffer);
		}
	} else if (n === 4) {
		ooura32.child_cftfsub(n, aBuffer, wBuffer);
	}
};

ooura32.child_bitrv2 = function (n, ipBuffer, ipOffset, aBuffer) {
	/* let */ var j, j1, k, k1, l, m;
	/* let */ var xr, xi, yr, yi;

    // Create some views on the raw buffers
	/* const */ var ip = new Int16Array(ipBuffer).subarray(ipOffset);
	/* const */ var a = new Float32Array(aBuffer);

	ip[0] = 0;
	l = n;
	m = 1;
	while ((m << 3) < l) {
		l >>= 1;
		for (j = 0; j < m; j++) {
			ip[m + j] = ip[j] + l;
		}
		m <<= 1;
	}
	/* const */ var m2 = 2 * m;
	if ((m << 3) === l) {
		for (k = 0; k < m; k++) {
			for (j = 0; j < k; j++) {
				j1 = 2 * j + ip[k];
				k1 = 2 * k + ip[j];
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += 2 * m2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 -= m2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += 2 * m2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
			j1 = 2 * k + m2 + ip[k];
			k1 = j1 + m2;
			xr = a[j1];
			xi = a[j1 + 1];
			yr = a[k1];
			yi = a[k1 + 1];
			a[j1] = yr;
			a[j1 + 1] = yi;
			a[k1] = xr;
			a[k1 + 1] = xi;
		}
	} else {
		for (k = 1; k < m; k++) {
			for (j = 0; j < k; j++) {
				j1 = 2 * j + ip[k];
				k1 = 2 * k + ip[j];
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += m2;
				xr = a[j1];
				xi = a[j1 + 1];
				yr = a[k1];
				yi = a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
		}
	}
};

ooura32.child_bitrv2conj = function (n, ipBuffer, ipOffset, aBuffer) {
	/* let */ var j, j1, k, k1, l, m;
	/* let */ var xr, xi, yr, yi;

	/* const */ var ip = new Int16Array(ipBuffer).subarray(ipOffset);
	/* const */ var a = new Float32Array(aBuffer);

	ip[0] = 0;
	l = n;
	m = 1;
	while ((m << 3) < l) {
		l >>= 1;
		for (j = 0; j < m; j++) {
			ip[m + j] = ip[j] + l;
		}
		m <<= 1;
	}
	/* const */ var m2 = 2 * m;
	if ((m << 3) === l) {
		for (k = 0; k < m; k++) {
			for (j = 0; j < k; j++) {
				j1 = 2 * j + ip[k];
				k1 = 2 * k + ip[j];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += 2 * m2;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 -= m2;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += 2 * m2;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
			k1 = 2 * k + ip[k];
			a[k1 + 1] = -a[k1 + 1];
			j1 = k1 + m2;
			k1 = j1 + m2;
			xr = a[j1];
			xi = -a[j1 + 1];
			yr = a[k1];
			yi = -a[k1 + 1];
			a[j1] = yr;
			a[j1 + 1] = yi;
			a[k1] = xr;
			a[k1 + 1] = xi;
			k1 += m2;
			a[k1 + 1] = -a[k1 + 1];
		}
	} else {
		a[1] = -a[1];
		a[m2 + 1] = -a[m2 + 1];
		for (k = 1; k < m; k++) {
			for (j = 0; j < k; j++) {
				j1 = 2 * j + ip[k];
				k1 = 2 * k + ip[j];
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += m2;
				xr = a[j1];
				xi = -a[j1 + 1];
				yr = a[k1];
				yi = -a[k1 + 1];
				a[j1] = yr;
				a[j1 + 1] = yi;
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
			k1 = 2 * k + ip[k];
			a[k1 + 1] = -a[k1 + 1];
			a[k1 + m2 + 1] = -a[k1 + m2 + 1];
		}
	}
};

ooura32.child_cftmdl = function (n, l, aBuffer, wBuffer) {
	/* let */ var j, j1, j2, j3, k, k1, k2;
	/* let */ var wk1r, wk1i, wk2r, wk2i, wk3r, wk3i;
	/* let */ var x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var w = new Float32Array(wBuffer);

	/* const */ var m = l << 2;
	for (j = 0; j < l; j += 2) {
		j1 = j + l;
		j2 = j1 + l;
		j3 = j2 + l;
		x0r = a[j] + a[j1];
		x0i = a[j + 1] + a[j1 + 1];
		x1r = a[j] - a[j1];
		x1i = a[j + 1] - a[j1 + 1];
		x2r = a[j2] + a[j3];
		x2i = a[j2 + 1] + a[j3 + 1];
		x3r = a[j2] - a[j3];
		x3i = a[j2 + 1] - a[j3 + 1];
		a[j] = x0r + x2r;
		a[j + 1] = x0i + x2i;
		a[j2] = x0r - x2r;
		a[j2 + 1] = x0i - x2i;
		a[j1] = x1r - x3i;
		a[j1 + 1] = x1i + x3r;
		a[j3] = x1r + x3i;
		a[j3 + 1] = x1i - x3r;
	}
	wk1r = w[2];
	for (j = m; j < l + m; j += 2) {
		j1 = j + l;
		j2 = j1 + l;
		j3 = j2 + l;
		x0r = a[j] + a[j1];
		x0i = a[j + 1] + a[j1 + 1];
		x1r = a[j] - a[j1];
		x1i = a[j + 1] - a[j1 + 1];
		x2r = a[j2] + a[j3];
		x2i = a[j2 + 1] + a[j3 + 1];
		x3r = a[j2] - a[j3];
		x3i = a[j2 + 1] - a[j3 + 1];
		a[j] = x0r + x2r;
		a[j + 1] = x0i + x2i;
		a[j2] = x2i - x0i;
		a[j2 + 1] = x0r - x2r;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j1] = wk1r * (x0r - x0i);
		a[j1 + 1] = wk1r * (x0r + x0i);
		x0r = x3i + x1r;
		x0i = x3r - x1i;
		a[j3] = wk1r * (x0i - x0r);
		a[j3 + 1] = wk1r * (x0i + x0r);
	}
	k1 = 0;
	/* const */ var m2 = 2 * m;
	for (k = m2; k < n; k += m2) {
		k1 += 2;
		k2 = 2 * k1;
		wk2r = w[k1];
		wk2i = w[k1 + 1];
		wk1r = w[k2];
		wk1i = w[k2 + 1];
		wk3r = wk1r - 2 * wk2i * wk1i;
		wk3i = 2 * wk2i * wk1r - wk1i;
		for (j = k; j < l + k; j += 2) {
			j1 = j + l;
			j2 = j1 + l;
			j3 = j2 + l;
			x0r = a[j] + a[j1];
			x0i = a[j + 1] + a[j1 + 1];
			x1r = a[j] - a[j1];
			x1i = a[j + 1] - a[j1 + 1];
			x2r = a[j2] + a[j3];
			x2i = a[j2 + 1] + a[j3 + 1];
			x3r = a[j2] - a[j3];
			x3i = a[j2 + 1] - a[j3 + 1];
			a[j] = x0r + x2r;
			a[j + 1] = x0i + x2i;
			x0r -= x2r;
			x0i -= x2i;
			a[j2] = wk2r * x0r - wk2i * x0i;
			a[j2 + 1] = wk2r * x0i + wk2i * x0r;
			x0r = x1r - x3i;
			x0i = x1i + x3r;
			a[j1] = wk1r * x0r - wk1i * x0i;
			a[j1 + 1] = wk1r * x0i + wk1i * x0r;
			x0r = x1r + x3i;
			x0i = x1i - x3r;
			a[j3] = wk3r * x0r - wk3i * x0i;
			a[j3 + 1] = wk3r * x0i + wk3i * x0r;
		}
		wk1r = w[k2 + 2];
		wk1i = w[k2 + 3];
		wk3r = wk1r - 2 * wk2r * wk1i;
		wk3i = 2 * wk2r * wk1r - wk1i;
		for (j = k + m; j < l + (k + m); j += 2) {
			j1 = j + l;
			j2 = j1 + l;
			j3 = j2 + l;
			x0r = a[j] + a[j1];
			x0i = a[j + 1] + a[j1 + 1];
			x1r = a[j] - a[j1];
			x1i = a[j + 1] - a[j1 + 1];
			x2r = a[j2] + a[j3];
			x2i = a[j2 + 1] + a[j3 + 1];
			x3r = a[j2] - a[j3];
			x3i = a[j2 + 1] - a[j3 + 1];
			a[j] = x0r + x2r;
			a[j + 1] = x0i + x2i;
			x0r -= x2r;
			x0i -= x2i;
			a[j2] = -wk2i * x0r - wk2r * x0i;
			a[j2 + 1] = -wk2i * x0i + wk2r * x0r;
			x0r = x1r - x3i;
			x0i = x1i + x3r;
			a[j1] = wk1r * x0r - wk1i * x0i;
			a[j1 + 1] = wk1r * x0i + wk1i * x0r;
			x0r = x1r + x3i;
			x0i = x1i - x3r;
			a[j3] = wk3r * x0r - wk3i * x0i;
			a[j3 + 1] = wk3r * x0i + wk3i * x0r;
		}
	}
};

ooura32.child_cft1st = function (n, aBuffer, wBuffer) {
	/* let */ var j, k1, k2;
	/* let */ var wk1r, wk1i, wk2r, wk2i, wk3r, wk3i;
	/* let */ var x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var w = new Float32Array(wBuffer);

	x0r = a[0] + a[2];
	x0i = a[1] + a[3];
	x1r = a[0] - a[2];
	x1i = a[1] - a[3];
	x2r = a[4] + a[6];
	x2i = a[5] + a[7];
	x3r = a[4] - a[6];
	x3i = a[5] - a[7];
	a[0] = x0r + x2r;
	a[1] = x0i + x2i;
	a[4] = x0r - x2r;
	a[5] = x0i - x2i;
	a[2] = x1r - x3i;
	a[3] = x1i + x3r;
	a[6] = x1r + x3i;
	a[7] = x1i - x3r;
	wk1r = w[2];
	x0r = a[8] + a[10];
	x0i = a[9] + a[11];
	x1r = a[8] - a[10];
	x1i = a[9] - a[11];
	x2r = a[12] + a[14];
	x2i = a[13] + a[15];
	x3r = a[12] - a[14];
	x3i = a[13] - a[15];
	a[8] = x0r + x2r;
	a[9] = x0i + x2i;
	a[12] = x2i - x0i;
	a[13] = x0r - x2r;
	x0r = x1r - x3i;
	x0i = x1i + x3r;
	a[10] = wk1r * (x0r - x0i);
	a[11] = wk1r * (x0r + x0i);
	x0r = x3i + x1r;
	x0i = x3r - x1i;
	a[14] = wk1r * (x0i - x0r);
	a[15] = wk1r * (x0i + x0r);
	k1 = 0;
	for (j = 16; j < n; j += 16) {
		k1 += 2;
		k2 = 2 * k1;
		wk2r = w[k1];
		wk2i = w[k1 + 1];
		wk1r = w[k2];
		wk1i = w[k2 + 1];
		wk3r = wk1r - 2 * wk2i * wk1i;
		wk3i = 2 * wk2i * wk1r - wk1i;
		x0r = a[j] + a[j + 2];
		x0i = a[j + 1] + a[j + 3];
		x1r = a[j] - a[j + 2];
		x1i = a[j + 1] - a[j + 3];
		x2r = a[j + 4] + a[j + 6];
		x2i = a[j + 5] + a[j + 7];
		x3r = a[j + 4] - a[j + 6];
		x3i = a[j + 5] - a[j + 7];
		a[j] = x0r + x2r;
		a[j + 1] = x0i + x2i;
		x0r -= x2r;
		x0i -= x2i;
		a[j + 4] = wk2r * x0r - wk2i * x0i;
		a[j + 5] = wk2r * x0i + wk2i * x0r;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j + 2] = wk1r * x0r - wk1i * x0i;
		a[j + 3] = wk1r * x0i + wk1i * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		a[j + 6] = wk3r * x0r - wk3i * x0i;
		a[j + 7] = wk3r * x0i + wk3i * x0r;
		wk1r = w[k2 + 2];
		wk1i = w[k2 + 3];
		wk3r = wk1r - 2 * wk2r * wk1i;
		wk3i = 2 * wk2r * wk1r - wk1i;
		x0r = a[j + 8] + a[j + 10];
		x0i = a[j + 9] + a[j + 11];
		x1r = a[j + 8] - a[j + 10];
		x1i = a[j + 9] - a[j + 11];
		x2r = a[j + 12] + a[j + 14];
		x2i = a[j + 13] + a[j + 15];
		x3r = a[j + 12] - a[j + 14];
		x3i = a[j + 13] - a[j + 15];
		a[j + 8] = x0r + x2r;
		a[j + 9] = x0i + x2i;
		x0r -= x2r;
		x0i -= x2i;
		a[j + 12] = -wk2i * x0r - wk2r * x0i;
		a[j + 13] = -wk2i * x0i + wk2r * x0r;
		x0r = x1r - x3i;
		x0i = x1i + x3r;
		a[j + 10] = wk1r * x0r - wk1i * x0i;
		a[j + 11] = wk1r * x0i + wk1i * x0r;
		x0r = x1r + x3i;
		x0i = x1i - x3r;
		a[j + 14] = wk3r * x0r - wk3i * x0i;
		a[j + 15] = wk3r * x0i + wk3i * x0r;
	}
};

ooura32.child_cftfsub = function (n, aBuffer, wBuffer) {
	/* let */ var j, j1, j2, j3, l;
	/* let */ var x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

    // Create some views on the raw buffers
	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var w = new Float32Array(wBuffer);

	l = 2;
	if (n > 8) {
		ooura32.child_cft1st(n, a.buffer, w.buffer);
		l = 8;
		while ((l << 2) < n) {
			ooura32.child_cftmdl(n, l, a.buffer, w.buffer);
			l <<= 2;
		}
	}
	if ((l << 2) === n) {
		for (j = 0; j < l; j += 2) {
			j1 = j + l;
			j2 = j1 + l;
			j3 = j2 + l;
			x0r = a[j] + a[j1];
			x0i = a[j + 1] + a[j1 + 1];
			x1r = a[j] - a[j1];
			x1i = a[j + 1] - a[j1 + 1];
			x2r = a[j2] + a[j3];
			x2i = a[j2 + 1] + a[j3 + 1];
			x3r = a[j2] - a[j3];
			x3i = a[j2 + 1] - a[j3 + 1];
			a[j] = x0r + x2r;
			a[j + 1] = x0i + x2i;
			a[j2] = x0r - x2r;
			a[j2 + 1] = x0i - x2i;
			a[j1] = x1r - x3i;
			a[j1 + 1] = x1i + x3r;
			a[j3] = x1r + x3i;
			a[j3 + 1] = x1i - x3r;
		}
	} else {
		for (j = 0; j < l; j += 2) {
			j1 = j + l;
			x0r = a[j] - a[j1];
			x0i = a[j + 1] - a[j1 + 1];
			a[j] += a[j1];
			a[j + 1] += a[j1 + 1];
			a[j1] = x0r;
			a[j1 + 1] = x0i;
		}
	}
};

ooura32.child_cftbsub = function (n, aBuffer, wBuffer) {
	/* let */ var j, j1, j2, j3, l;
	/* let */ var x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var w = new Float32Array(wBuffer);

	l = 2;
	if (n > 8) {
		ooura32.child_cft1st(n, a.buffer, w.buffer);
		l = 8;
		while ((l << 2) < n) {
			ooura32.child_cftmdl(n, l, a.buffer, w.buffer);
			l <<= 2;
		}
	}
	if ((l << 2) === n) {
		for (j = 0; j < l; j += 2) {
			j1 = j + l;
			j2 = j1 + l;
			j3 = j2 + l;
			x0r = a[j] + a[j1];
			x0i = -a[j + 1] - a[j1 + 1];
			x1r = a[j] - a[j1];
			x1i = -a[j + 1] + a[j1 + 1];
			x2r = a[j2] + a[j3];
			x2i = a[j2 + 1] + a[j3 + 1];
			x3r = a[j2] - a[j3];
			x3i = a[j2 + 1] - a[j3 + 1];
			a[j] = x0r + x2r;
			a[j + 1] = x0i - x2i;
			a[j2] = x0r - x2r;
			a[j2 + 1] = x0i + x2i;
			a[j1] = x1r - x3i;
			a[j1 + 1] = x1i - x3r;
			a[j3] = x1r + x3i;
			a[j3 + 1] = x1i + x3r;
		}
	} else {
		for (j = 0; j < l; j += 2) {
			j1 = j + l;
			x0r = a[j] - a[j1];
			x0i = -a[j + 1] + a[j1 + 1];
			a[j] += a[j1];
			a[j + 1] = -a[j + 1] - a[j1 + 1];
			a[j1] = x0r;
			a[j1 + 1] = x0i;
		}
	}
};

ooura32.child_rftfsub = function (n, aBuffer, nc, cBuffer, cOffset) {
	/* let */ var j, k, kk;
	/* let */ var wkr, wki, xr, xi, yr, yi;

	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var c = new Float32Array(cBuffer).subarray(cOffset);

	/* const */ var m = n >> 1;
	/* const */ var ks = 2 * nc / m;
	kk = 0;
	for (j = 2; j < m; j += 2) {
		k = n - j;
		kk += ks;
		wkr = 0.5 - c[nc - kk];
		wki = c[kk];
		xr = a[j] - a[k];
		xi = a[j + 1] + a[k + 1];
		yr = wkr * xr - wki * xi;
		yi = wkr * xi + wki * xr;
		a[j] -= yr;
		a[j + 1] -= yi;
		a[k] += yr;
		a[k + 1] -= yi;
	}
};

ooura32.child_rftbsub = function (n, aBuffer, nc, cBuffer, cOffset) {
	/* let */ var j, k, kk;
	/* let */ var wkr, wki, xr, xi, yr, yi;

	/* const */ var a = new Float32Array(aBuffer);
	/* const */ var c = new Float32Array(cBuffer).subarray(cOffset);

	a[1] = -a[1];
	/* const */ var m = n >> 1;
	/* const */ var ks = 2 * nc / m;
	kk = 0;
	for (j = 2; j < m; j += 2) {
		k = n - j;
		kk += ks;
		wkr = 0.5 - c[nc - kk];
		wki = c[kk];
		xr = a[j] - a[k];
		xi = a[j + 1] + a[k + 1];
		yr = wkr * xr + wki * xi;
		yi = wkr * xi - wki * xr;
		a[j] -= yr;
		a[j + 1] = yi - a[j + 1];
		a[k] += yr;
		a[k + 1] = yi - a[k + 1];
	}
	a[m + 1] = -a[m + 1];
};
