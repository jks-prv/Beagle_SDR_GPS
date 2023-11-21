// ----------------------------------------------------------------------------
// mfsk-pic.cxx  --  mfsk support functions
//
// Copyright (C) 2006-2008
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.  Adapted from code contained in gmfsk source code 
// distribution.
//  gmfsk Copyright (C) 2001, 2002, 2003
//  Tomi Manninen (oh2bns@sral.fi)
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include "gettext.h"

Fl_Double_Window	*picRxWin = (Fl_Double_Window *)0;

picture		*picRx = (picture *)0;

Fl_Double_Window	*picTxWin = (Fl_Double_Window *)0;

picture		*picTx = (picture *)0;
picbox		*picTxBox = 0;
Fl_Button	*btnpicTxSPP = (Fl_Button *)0;
Fl_Button	*btnpicTxSendColor = (Fl_Button *)0;
Fl_Button	*btnpicTxSendGrey = (Fl_Button *)0;
Fl_Button	*btnpicTxSendAbort = (Fl_Button *)0;
Fl_Button	*btnpicTxLoad = (Fl_Button *)0;
Fl_Button	*btnpicTxClose = (Fl_Button *)0;

Fl_Shared_Image	*TxImg = (Fl_Shared_Image *)0;
unsigned char *xmtimg = (unsigned char *)0;
unsigned char *xmtpicbuff = (unsigned char *)0;

mfsk *serviceme = 0;
int	txSPP = 8;

char txclr_tooltip[24];
char txgry_tooltip[24];

void updateRxPic(unsigned char data, int pos)
{
	picRx->pixel(data, pos);
}

void createRxViewer()
{
	picRxWin = new Fl_Double_Window(200, 140);
	picRxWin->xclass(PACKAGE_NAME);
	picRxWin->begin();
	picRx = new picture(2, 2, 136, 104);
	picRxWin->end();
}

void showRxViewer(int W, int H)
{
	if (!picRxWin) createRxViewer();
	int winW, winH;
	int picX, picY;
	winW = W < 136 ? 140 : W + 4;
	winH = H + 4;
	picX = (winW - W) / 2;
	picY = 2;
	picRxWin->size(winW, winH);
	picRx->resize(picX, picY, W, H);
	picRx->clear();
	picRxWin->show();
}

int load_image(const char *n) {
	
	if (serviceme != active_modem) {
		return 0;
	}

	int W, H, D;
	unsigned char *img_data;
	
	if (TxImg) {
		TxImg->release();
		TxImg = 0;
	}
	TxImg = Fl_Shared_Image::get(n);

	if (!TxImg)
		return 0;

	if (TxImg->count() > 1) {
		TxImg->release();
		TxImg = 0;
		return 0;
	}

	img_data = (unsigned char *)TxImg->data()[0];
	W = TxImg->w();
	H = TxImg->h();
	D = TxImg->d();
	if (xmtimg) delete [] xmtimg;
	xmtimg = new unsigned char [W * H * 3];
	if (D == 3)
		memcpy(xmtimg, img_data, W*H*3);
	else if (D == 4) {
		int i, j, k;
		for (i = 0; i < W*H; i++) {
			j = i*3; k = i*4;
			xmtimg[j] = img_data[k];
			xmtimg[j+1] = img_data[k+1];
			xmtimg[j+2] = img_data[k+2];
		}
	} else if (D == 1) {
		int i, j;
		for (i = 0; i < W*H; i++) {
			j = i * 3;
			xmtimg[j] = xmtimg[j+1] = xmtimg[j+2] = img_data[i];
		}
	} else
		return 0;

	TxViewerResize(W, H);
	char* label = strdup(n);
	picTxWin->copy_label(basename(label));
	free(label);
	picTxBox->label(0);
// load the picture widget with the rgb image
	picTx->show();
	picTx->clear();
	picTxWin->redraw();
	picTx->video(xmtimg, W * H * 3);
	if (print_time_left( (W * H * 3) * 0.000125 * serviceme->TXspp, 
		txclr_tooltip, sizeof(txclr_tooltip), _("Time needed: ")) > 0)
		btnpicTxSendColor->tooltip(txclr_tooltip);
	btnpicTxSendColor->activate();
	if (print_time_left( (W * H) * 0.000125 * serviceme->TXspp, 
		txgry_tooltip, sizeof(txgry_tooltip), _("Time needed: ")) > 0)
		btnpicTxSendGrey->tooltip(txgry_tooltip);
	btnpicTxSendGrey->activate();
	return 1;
}

void updateTxPic(unsigned char data)
{
	if (serviceme != active_modem) return;
	if (serviceme->color) {
		serviceme->pixelnbr = serviceme->rgb + serviceme->row + 3*serviceme->col;
		picTx->pixel(data, serviceme->pixelnbr);
		if (++serviceme->col == TxImg->w()) {
			serviceme->col = 0;
			if (++serviceme->rgb == 3) {
				serviceme->rgb = 0;
				serviceme->row += 3 * TxImg->w();
			}
		}
	} else {
		picTx->pixel( data, serviceme->pixelnbr++ );
		picTx->pixel( data, serviceme->pixelnbr++ );
		picTx->pixel( data, serviceme->pixelnbr++ );
	}
}

void cb_picTxLoad(Fl_Widget *, void *)
{
	const char *fn =
		FSEL::select(_("Load image file"), "Portable Network Graphics\t*.png\n"
			    "Independent JPEG Group\t*.{jpg,jif,jpeg,jpe}\n"
			    "Graphics Interchange Format\t*.gif", PicsDir.c_str());
	if (!fn) return;
	if (!*fn) return;
	load_image(fn);
}

void cb_picTxClose( Fl_Widget *w, void *)
{
	picTxWin->hide();
}

void pic_TxSendColor()
{
	int W, H, rowstart;
	W = TxImg->w();
	H = TxImg->h();
	if (xmtpicbuff) delete [] xmtpicbuff;
	xmtpicbuff = new unsigned char [W*H*3];
	unsigned char *outbuf = xmtpicbuff;
	unsigned char *inbuf = xmtimg;
	int iy, ix, rgb;
	for (iy = 0; iy < H; iy++) {
		rowstart = iy * W * 3;
		for (rgb = 0; rgb < 3; rgb++)
			for (ix = 0; ix < W; ix++)
				outbuf[rowstart + rgb*W + ix] = inbuf[rowstart + rgb + ix*3];
	}
	if (serviceme->TXspp == 8)
		snprintf(serviceme->picheader, PICHEADER, "\nSending Pic:%dx%dC;", W, H);
	else
		snprintf(serviceme->picheader, PICHEADER, "\nSending Pic:%dx%dCp%d;", W, H,serviceme->TXspp);	
	serviceme->xmtbytes = W * H * 3;
	serviceme->color = true;
	serviceme->rgb = 0;
	serviceme->col = 0;
	serviceme->row = 0;
	serviceme->pixelnbr = 0;
	btnpicTxSPP->hide();
	btnpicTxSendColor->hide();
	btnpicTxSendGrey->hide();
	btnpicTxLoad->hide();
	btnpicTxClose->hide();
	btnpicTxSendAbort->show();
	picTx->clear();
	if (!picTxWin->visible())
		picTxWin->show();
// start the transmission
	start_tx();
	serviceme->startpic = true;
}

void cb_picTxSendColor( Fl_Widget *w, void *)
{
	if (serviceme != active_modem) return;
	pic_TxSendColor();
}

void pic_TxSendGrey()
{
	int W, H;
	W = TxImg->w();
	H = TxImg->h();
	if (xmtpicbuff) delete [] xmtpicbuff;
	xmtpicbuff = new unsigned char [W*H];
	unsigned char *outbuf = xmtpicbuff;
	unsigned char *inbuf = xmtimg;
	for (int i = 0; i < W*H; i++)
		outbuf[i] = ( 31 * inbuf[i*3] + 61 * inbuf[i*3 + 1] + 8 * inbuf[i*3 + 2])/100;
	if (serviceme->TXspp == 8)
		snprintf(serviceme->picheader, PICHEADER, "\nSending Pic:%dx%d;", W, H);
	else
		snprintf(serviceme->picheader, PICHEADER, "\nSending Pic:%dx%dp%d;", W, H,serviceme->TXspp);	
	serviceme->xmtbytes = W * H;
	serviceme->color = false;
	serviceme->col = 0;
	serviceme->row = 0;
	serviceme->pixelnbr = 0;
	btnpicTxSPP->hide();
	btnpicTxSendColor->hide();
	btnpicTxSendGrey->hide();
	btnpicTxLoad->hide();
	btnpicTxClose->hide();
	btnpicTxSendAbort->show();
	picTx->clear();
// start the transmission
	if (!picTxWin->visible())
		picTxWin->show();
	start_tx();
	serviceme->startpic = true;
}

void cb_picTxSendGrey( Fl_Widget *w, void *)
{
	if (serviceme != active_modem) return;
	pic_TxSendGrey();
}

void cb_picTxSendAbort( Fl_Widget *w, void *)
{
	if (serviceme != active_modem) return;

	serviceme->abortxmt = true;
// reload the picture widget with the rgb image
	picTx->video(xmtimg, TxImg->w() * TxImg->h() * 3);
}

void cb_picTxSPP( Fl_Widget *w, void *)
{
	if (serviceme != active_modem) return;

	Fl_Button *b = (Fl_Button *)w;

	if (serviceme->TXspp == 8) serviceme->TXspp = 4;
	else if (serviceme->TXspp == 4) serviceme->TXspp = 2;
	else serviceme->TXspp = 8;

	if (serviceme->TXspp == 8) b->label("X1");
	else if (serviceme->TXspp == 4) b->label("X2");
	else b->label("X4");
	b->redraw_label();
	txSPP = serviceme->TXspp;
	
	if (TxImg == 0) return;
	if (TxImg->w() > 0 && TxImg->h() > 0) {
		if (print_time_left( (TxImg->w() * TxImg->h() * 3) * 0.000125 * serviceme->TXspp, 
			txclr_tooltip, sizeof(txclr_tooltip), _("Time needed: ")) > 0)
			btnpicTxSendColor->tooltip(txclr_tooltip);
		if (print_time_left( (TxImg->w() * TxImg->h()) * 0.000125 * serviceme->TXspp, 
			txgry_tooltip, sizeof(txgry_tooltip), _("Time needed: ")) > 0)
			btnpicTxSendGrey->tooltip(txgry_tooltip);
	}
}

void createTxViewer()
{
	picTxWin = new Fl_Double_Window(290, 180, _("Send image"));
	picTxWin->xclass(PACKAGE_NAME);
	picTxWin->begin();

	picTx = new picture (2, 2, 286, 150);
	picTx->hide();
	picTxBox = new picbox(picTxWin->x(), picTxWin->y(), picTxWin->w(), picTxWin->h(),
			      _("Load or drop an image file\nSupported types: PNG, JPEG, BMP"));
	picTxBox->labelfont(FL_HELVETICA_ITALIC);
	
	btnpicTxSPP = new Fl_Button(5, 180 - 26, 40, 24, "X1");
	btnpicTxSPP->tooltip(_("Transfer speed, X1-normal"));
	btnpicTxSPP->callback( cb_picTxSPP, 0);
		
	btnpicTxSendColor = new Fl_Button(45, 180 - 26, 60, 24, "XmtClr");
	btnpicTxSendColor->callback(cb_picTxSendColor, 0);

	btnpicTxSendGrey = new Fl_Button(105, 180 - 26, 60, 24, "XmtGry");
	btnpicTxSendGrey->callback( cb_picTxSendGrey, 0);

	btnpicTxSendAbort = new Fl_Button(84, 180 - 26, 122, 24, "Abort Xmt");
	btnpicTxSendAbort->callback(cb_picTxSendAbort, 0);

	btnpicTxLoad = new Fl_Button(165, 180 - 26, 60, 24, _("Load"));
	btnpicTxLoad->callback(cb_picTxLoad, 0);

	btnpicTxClose = new Fl_Button(225, 180 - 26, 60, 24, _("Close"));
	btnpicTxClose->callback(cb_picTxClose, 0);

	btnpicTxSendAbort->hide();
	btnpicTxSendColor->deactivate();
	btnpicTxSendGrey->deactivate();

	picTxWin->end();
}

void TxViewerResize(int W, int H)
{
	int winW, winH;
	int picX, picY;
	winW = W < 286 ? 290 : W + 4;
	winH = H < 180 ? 210 : H + 30;
	picX = (winW - W) / 2;
	picY = (winH - 26 - H) / 2;
	picTxWin->size(winW, winH);
	picTx->resize(picX, picY, W, H);
	picTx->clear();
	picTxBox->size(winW, winH);
	btnpicTxSPP->resize(winW/2 - 140, winH - 26, 40, 24);
	btnpicTxSendColor->resize(winW/2 - 100, winH - 26, 60, 24);
	btnpicTxSendGrey->resize(winW/2 - 40, winH - 26, 60, 24);
	btnpicTxSendAbort->resize(winW/2 - 61, winH - 26, 122, 24);
	btnpicTxLoad->resize(winW/2 + 20, winH - 26, 60, 24);
	btnpicTxClose->resize(winW/2 + 80, winH - 26, 60, 24);
}

void showTxViewer(int W, int H)
{
	if (picTxWin) {
		picTxWin->show();
		return;
	}

	int winW, winH;
	int picX, picY;
	winW = W < 288 ? 290 : W + 4;
	winH = H < 180 ? 180 : H + 30;
	picX = (winW - W) / 2;
	picY = (winH - 26 - H) / 2;
	picTxWin->size(winW, winH);
	picTx->resize(picX, picY, W, H);
	btnpicTxSPP->resize(winW/2 - 140, winH - 26, 40, 24);
	btnpicTxSendColor->resize(winW/2 - 100, winH - 26, 60, 24);
	btnpicTxSendGrey->resize(winW/2 - 40, winH - 26, 60, 24);
	btnpicTxSendAbort->resize(winW/2 - 61, winH - 26, 122, 24);
	btnpicTxLoad->resize(winW/2 + 20, winH - 26, 60, 24);
	btnpicTxClose->resize(winW/2 + 80, winH - 26, 60, 24);
	btnpicTxSPP->show();
	btnpicTxSendColor->show();
	btnpicTxSendGrey->show();
	btnpicTxLoad->show();
	btnpicTxClose->show();
	btnpicTxSendAbort->hide();
	picTxWin->show();
}

void deleteTxViewer()
{
	picTxWin->hide();
	if (picTx) delete picTx;
	delete [] xmtimg;
	xmtimg = 0;
	delete [] xmtpicbuff;
	xmtpicbuff = 0;
	delete picTxWin;
	picTxWin = 0;
	serviceme = 0;
}

void deleteRxViewer()
{
	picRxWin->hide();
	if (picRx) {
		delete picRx;
		picRx = 0;
	}
	delete picRxWin;
	picRxWin = 0;
	serviceme = 0;
}

int print_time_left(float time_sec, char *str, size_t len,
			  const char *prefix, const char *suffix)
{
	int time_min = (int)(time_sec / 60);
	time_sec -= time_min * 60;

	if (time_min)
		return snprintf(str, len, "%s %02dm %2.1fs%s",
				prefix, time_min, time_sec, suffix);
	else
		return snprintf(str, len, "%s %2.1fs%s", prefix, time_sec, suffix);
}

void setpicture_link(mfsk *me)
{
	serviceme = me;
}

