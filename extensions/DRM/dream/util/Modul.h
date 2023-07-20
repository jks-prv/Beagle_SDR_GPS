/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001
 *
 * Author(s):
 *	Volker Fischer
 *
 * Description:
 *	High level class for all modules. The common functionality for reading
 *	and writing the transfer-buffers are implemented here.
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

#if !defined(AFX_MODUL_H__41E39CD3_2AEC_400E_907B_148C0EC17A43__INCLUDED_)
#define AFX_MODUL_H__41E39CD3_2AEC_400E_907B_148C0EC17A43__INCLUDED_

#include "Buffer.h"
#include "Vector.h"
#include "../Parameter.h"
#include <iostream>


/* Classes ********************************************************************/
/* CModul ------------------------------------------------------------------- */
template<class TInput, class TOutput>
class CModul
{
public:
	CModul();
	virtual ~CModul() {}

	virtual void Init(CParameter& Parameter);
	virtual void Init(CParameter& Parameter, CBuffer<TOutput>& OutputBuffer);

protected:
	CVectorEx<TInput>*	pvecInputData;
	CVectorEx<TOutput>*	pvecOutputData;

	/* Max block-size are used to determine the size of the required buffer */
	int					iMaxOutputBlockSize;
	/* Actual read (or written) size of the data */
	int					iInputBlockSize;
	int					iOutputBlockSize;

	void				Lock() {Mutex.Lock();}
	void				Unlock() {Mutex.Unlock();}

	void				InitThreadSave(CParameter& Parameter);
	virtual void		InitInternal(CParameter& Parameter) = 0;
	void				ProcessDataThreadSave(CParameter& Parameter);
	virtual void		ProcessDataInternal(CParameter& Parameter) = 0;

private:
	CMutex				Mutex;
};


/* CTransmitterModul -------------------------------------------------------- */
template<class TInput, class TOutput>
class CTransmitterModul : public CModul<TInput, TOutput>
{
public:
	CTransmitterModul();
	virtual ~CTransmitterModul() {}

	virtual void		Init(CParameter& Parameter);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer);
	virtual void		ReadData(CParameter& Parameter,
								 CBuffer<TOutput>& OutputBuffer);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TOutput>& OutputBuffer);
	virtual void		ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TInput>& InputBuffer2,
									CBuffer<TInput>& InputBuffer3,
									CBuffer<TOutput>& OutputBuffer);
	virtual bool	WriteData(CParameter& Parameter,
								  CBuffer<TInput>& InputBuffer);

protected:
	/* Additional buffers if the derived class has multiple input streams */
	CVectorEx<TInput>*	pvecInputData2;
	CVectorEx<TInput>*	pvecInputData3;

	/* Actual read (or written) size of the data */
	int					iInputBlockSize2;
	int					iInputBlockSize3;
};


/* CReceiverModul ----------------------------------------------------------- */
template<class TInput, class TOutput>
class CReceiverModul : public CModul<TInput, TOutput>
{
public:
	CReceiverModul();
	virtual ~CReceiverModul() {}

	void				SetInitFlag() {bDoInit = true;}
	virtual void		Init(CParameter& Parameter);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer,
							 CBuffer<TOutput>& OutputBuffer2);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer,
							 CBuffer<TOutput>& OutputBuffer2,
							 CBuffer<TOutput>& OutputBuffer3);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer,
							 CBuffer<TOutput>& OutputBuffer2,
							 std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer);
	virtual void		Init(CParameter& Parameter,
							 std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer);
	virtual void		ReadData(CParameter& Parameter,
								 CBuffer<TOutput>& OutputBuffer);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TOutput>& OutputBuffer);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TOutput>& OutputBuffer,
									CBuffer<TOutput>& OutputBuffer2);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TOutput>& OutputBuffer,
									CBuffer<TOutput>& OutputBuffer2,
									CBuffer<TOutput>& OutputBuffer3);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									CBuffer<TOutput>& OutputBuffer,
									CBuffer<TOutput>& OutputBuffer2,
									std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer);
	virtual bool	ProcessData(CParameter& Parameter,
									CBuffer<TInput>& InputBuffer,
									std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer);
	virtual bool	WriteData(CParameter& Parameter,
								  CBuffer<TInput>& InputBuffer);
	void                SetTrigger() { trigger = 1; }

protected:
	void SetBufReset1() {bResetBuf = true;}
	void SetBufReset2() {bResetBuf2 = true;}
	void SetBufReset3() {bResetBuf3 = true;}
	void SetBufResetN() {for(size_t i=0; i<vecbResetBuf.size(); i++)
     vecbResetBuf[i] = true;}

	/* Additional buffers if the derived class has multiple output streams */
	CVectorEx<TOutput>*	pvecOutputData2;
	CVectorEx<TOutput>*	pvecOutputData3;
	std::vector<CVectorEx<TOutput>*>	vecpvecOutputData;

	/* Max block-size are used to determine the size of the required buffer */
	int					iMaxOutputBlockSize2;
	int					iMaxOutputBlockSize3;
	std::vector<int>			veciMaxOutputBlockSize;
	/* Actual read (or written) size of the data */
	int					iOutputBlockSize2;
	int					iOutputBlockSize3;
	std::vector<int>			veciOutputBlockSize;

private:
	/* Init flag */
	bool			bDoInit;

	/* Reset flags for output cyclic-buffers */
	bool			bResetBuf;
	bool			bResetBuf2;
	bool			bResetBuf3;
	std::vector<bool>	vecbResetBuf;
	
	int trigger;
};


/* CSimulationModul --------------------------------------------------------- */
template<class TInput, class TOutput, class TInOut2>
class CSimulationModul : public CModul<TInput, TOutput>
{
public:
	CSimulationModul();
	virtual ~CSimulationModul() {}

	virtual void		Init(CParameter& Parameter);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer);
	virtual void		Init(CParameter& Parameter,
							 CBuffer<TOutput>& OutputBuffer,
							 CBuffer<TInOut2>& OutputBuffer2);
	virtual void		TransferData(CParameter& Parameter,
									 CBuffer<TInput>& InputBuffer,
									 CBuffer<TOutput>& OutputBuffer);


// TEST "ProcessDataIn" "ProcessDataOut"
	virtual bool	ProcessDataIn(CParameter& Parameter,
									  CBuffer<TInput>& InputBuffer,
									  CBuffer<TInOut2>& InputBuffer2,
									  CBuffer<TOutput>& OutputBuffer);
	virtual bool	ProcessDataOut(CParameter& Parameter,
									   CBuffer<TInput>& InputBuffer,
									   CBuffer<TOutput>& OutputBuffer,
									   CBuffer<TInOut2>& OutputBuffer2);


protected:
	/* Additional buffers if the derived class has multiple output streams */
	CVectorEx<TInOut2>*	pvecOutputData2;

	/* Max block-size are used to determine the size of the requiered buffer */
	int					iMaxOutputBlockSize2;
	/* Actual read (or written) size of the data */
	int					iOutputBlockSize2;

	/* Additional buffers if the derived class has multiple input streams */
	CVectorEx<TInOut2>*	pvecInputData2;

	/* Actual read (or written) size of the data */
	int					iInputBlockSize2;
};


/* Implementation *************************************************************/
/******************************************************************************\
* CModul                                                                       *
\******************************************************************************/
template<class TInput, class TOutput>
CModul<TInput, TOutput>::CModul()
{
	/* Initialize everything with zeros */
	iMaxOutputBlockSize = 0;
	iInputBlockSize = 0;
	iOutputBlockSize = 0;
	pvecInputData = nullptr;
	pvecOutputData = nullptr;
}

template<class TInput, class TOutput>
void CModul<TInput, TOutput>::ProcessDataThreadSave(CParameter& Parameter)
{
	/* Get a lock for the resources */
	Lock();

	/* Call processing routine of derived modul */
	ProcessDataInternal(Parameter);

	/* Unlock resources */
	Unlock();
}

template<class TInput, class TOutput>
void CModul<TInput, TOutput>::InitThreadSave(CParameter& Parameter)
{
	/* Get a lock for the resources */
	Lock();

	try
	{
		/* Call init of derived modul */
		InitInternal(Parameter);

		/* Unlock resources */
		Unlock();
	}

	catch (CGenErr)
	{
		/* Unlock resources */
		Unlock();

		/* Throws the same error again which was send by the function */
		throw;
	}
}

template<class TInput, class TOutput>
void CModul<TInput, TOutput>::Init(CParameter& Parameter)
{
	/* Init some internal variables */
	iInputBlockSize = 0;

	/* Call init of derived modul */
	InitThreadSave(Parameter);
}

template<class TInput, class TOutput>
void CModul<TInput, TOutput>::Init(CParameter& Parameter,
								   CBuffer<TOutput>& OutputBuffer)
{
	/* Init some internal variables */
	iMaxOutputBlockSize = 0;
	iInputBlockSize = 0;
	iOutputBlockSize = 0;

	/* Call init of derived modul */
	InitThreadSave(Parameter);

	/* Init output transfer buffer */
	if (iMaxOutputBlockSize != 0)
		OutputBuffer.Init(iMaxOutputBlockSize);
	else
	{
		if (iOutputBlockSize != 0)
			OutputBuffer.Init(iOutputBlockSize);
	}
}


/******************************************************************************\
* Transmitter modul (CTransmitterModul)                                        *
\******************************************************************************/
template<class TInput, class TOutput>
CTransmitterModul<TInput, TOutput>::CTransmitterModul()
{
	/* Initialize all member variables with zeros */
	iInputBlockSize2 = 0;
	iInputBlockSize3 = 0;
	pvecInputData2 = nullptr;
	pvecInputData3 = nullptr;
}

template<class TInput, class TOutput>
void CTransmitterModul<TInput, TOutput>::Init(CParameter& Parameter)
{
	/* Init some internal variables */
	iInputBlockSize2 = 0;
	iInputBlockSize3 = 0;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter);
}

template<class TInput, class TOutput>
void CTransmitterModul<TInput, TOutput>::Init(CParameter& Parameter,
											  CBuffer<TOutput>& OutputBuffer)
{
	/* Init some internal variables */
	iInputBlockSize2 = 0;
	iInputBlockSize3 = 0;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);
}

template<class TInput, class TOutput>
bool CTransmitterModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TOutput>& OutputBuffer)
{
	/* OUTPUT-DRIVEN modul implementation in the transmitter ---------------- */
	/* Look in output buffer if data is requested */
	if (OutputBuffer.GetRequestFlag())
	{
		/* Check, if enough input data is available */
		if (InputBuffer.GetFillLevel() < this->iInputBlockSize)
		{
			/* Set request flag */
			InputBuffer.SetRequestFlag(true);

			return false;
		}

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

		/* Copy extended data from std::vectors */
		(*(this->pvecOutputData)).
			SetExData((*(this->pvecInputData)).GetExData());

		/* Call the underlying processing-routine */
		this->ProcessDataInternal(Parameter);

		/* Write processed data from internal memory in transfer-buffer */
		OutputBuffer.Put(this->iOutputBlockSize);

		/* Data was provided, clear data request */
		OutputBuffer.SetRequestFlag(false);
	}

	return true;
}

template<class TInput, class TOutput>
void CTransmitterModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TInput>& InputBuffer2,
				CBuffer<TInput>& InputBuffer3,
				CBuffer<TOutput>& OutputBuffer)
{
	/* OUTPUT-DRIVEN modul implementation in the transmitter ---------------- */
	/* Look in output buffer if data is requested */
	if (OutputBuffer.GetRequestFlag())
	{
		/* Check, if enough input data is available from all sources */
		if (InputBuffer.GetFillLevel() < this->iInputBlockSize)
		{
			/* Set request flag */
			InputBuffer.SetRequestFlag(true);

			return;
		}
		if (InputBuffer2.GetFillLevel() < iInputBlockSize2)
		{
			/* Set request flag */
			InputBuffer2.SetRequestFlag(true);

			return;
		}
		if (InputBuffer3.GetFillLevel() < iInputBlockSize3)
		{
			/* Set request flag */
			InputBuffer3.SetRequestFlag(true);

			return;
		}

		/* Get std::vectors from transfer-buffers */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);
		pvecInputData2 = InputBuffer2.Get(iInputBlockSize2);
		pvecInputData3 = InputBuffer3.Get(iInputBlockSize3);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

		/* Call the underlying processing-routine */
		this->ProcessDataInternal(Parameter);

		/* Write processed data from internal memory in transfer-buffer */
		OutputBuffer.Put(this->iOutputBlockSize);

		/* Data was provided, clear data request */
		OutputBuffer.SetRequestFlag(false);
	}
}

template<class TInput, class TOutput>
void CTransmitterModul<TInput, TOutput>::
	ReadData(CParameter& Parameter, CBuffer<TOutput>& OutputBuffer)
{
	/* OUTPUT-DRIVEN modul implementation in the transmitter ---------------- */
	/* Look in output buffer if data is requested */
	if (OutputBuffer.GetRequestFlag())
	{
		/* Read data and write it in the transfer-buffer.
		   Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

		/* Call the underlying processing-routine */
		this->ProcessDataInternal(Parameter);

		/* Write processed data from internal memory in transfer-buffer */
		OutputBuffer.Put(this->iOutputBlockSize);

		/* Data was provided, clear data request */
		OutputBuffer.SetRequestFlag(false);
	}
}

template<class TInput, class TOutput>
bool CTransmitterModul<TInput, TOutput>::
	WriteData(CParameter& Parameter, CBuffer<TInput>& InputBuffer)
{
	/* OUTPUT-DRIVEN modul implementation in the transmitter */
	/* Check, if enough input data is available */
	if (InputBuffer.GetFillLevel() < this->iInputBlockSize)
	{
		/* Set request flag */
		InputBuffer.SetRequestFlag(true);

		return false;
	}

	/* Get std::vector from transfer-buffer */
	this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

	/* Call the underlying processing-routine */
	this->ProcessDataInternal(Parameter);

	return true;
}


/******************************************************************************\
* Receiver modul (CReceiverModul)                                              *
\******************************************************************************/
template<class TInput, class TOutput>
CReceiverModul<TInput, TOutput>::CReceiverModul()
{
	/* Initialize all member variables with zeros */
	iMaxOutputBlockSize2 = 0;
	iMaxOutputBlockSize3 = 0;
	iOutputBlockSize2 = 0;
	iOutputBlockSize3 = 0;
	pvecOutputData2 = nullptr;
	pvecOutputData3 = nullptr;
	bResetBuf = false;
	bResetBuf2 = false;
	bResetBuf3 = false;
	bDoInit = false;
	trigger = 0;
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter)
{
	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter);
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter,
									  CBuffer<TOutput>& OutputBuffer)
{
	/* Init flag */
	bResetBuf = false;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter,
									  CBuffer<TOutput>& OutputBuffer,
									  CBuffer<TOutput>& OutputBuffer2)
{
	/* Init some internal variables */
	iMaxOutputBlockSize2 = 0;
	iOutputBlockSize2 = 0;
	bResetBuf = false;
	bResetBuf2 = false;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);

	/* Init output transfer buffers */
	if (iMaxOutputBlockSize2 != 0)
		OutputBuffer2.Init(iMaxOutputBlockSize2);
	else
	{
		if (iOutputBlockSize2 != 0)
			OutputBuffer2.Init(iOutputBlockSize2);
	}
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter,
									  CBuffer<TOutput>& OutputBuffer,
									  CBuffer<TOutput>& OutputBuffer2,
									  CBuffer<TOutput>& OutputBuffer3)
{
	/* Init some internal variables */
	iMaxOutputBlockSize2 = 0;
	iMaxOutputBlockSize3 = 0;
	iOutputBlockSize2 = 0;
	iOutputBlockSize3 = 0;
	bResetBuf = false;
	bResetBuf2 = false;
	bResetBuf3 = false;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);

	/* Init output transfer buffers */
	if (iMaxOutputBlockSize2 != 0)
		OutputBuffer2.Init(iMaxOutputBlockSize2);
	else
	{
		if (iOutputBlockSize2 != 0)
			OutputBuffer2.Init(iOutputBlockSize2);
	}

	if (iMaxOutputBlockSize3 != 0)
		OutputBuffer3.Init(iMaxOutputBlockSize3);
	else
	{
		if (iOutputBlockSize3 != 0)
			OutputBuffer3.Init(iOutputBlockSize3);
	}
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter,
									  CBuffer<TOutput>& OutputBuffer,
									  CBuffer<TOutput>& OutputBuffer2,
									  std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer)
{
	size_t i;
	/* Init some internal variables */
	iMaxOutputBlockSize2 = 0;
	iMaxOutputBlockSize3 = 0;
	veciMaxOutputBlockSize.resize(vecOutputBuffer.size());
    for(i=0; i<veciMaxOutputBlockSize.size(); i++)
		veciMaxOutputBlockSize[i]=0;
	iOutputBlockSize2 = 0;
	iOutputBlockSize3 = 0;
	veciOutputBlockSize.resize(vecOutputBuffer.size());
    for(i=0; i<veciOutputBlockSize.size(); i++)
		veciOutputBlockSize[i]=0;
	bResetBuf = false;
	bResetBuf2 = false;
	bResetBuf3 = false;
	vecbResetBuf.resize(vecOutputBuffer.size());
    for(i=0; i<vecbResetBuf.size(); i++)
		vecbResetBuf[i]=false;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);

	/* Init output transfer buffers */
	if (iMaxOutputBlockSize2 != 0)
		OutputBuffer2.Init(iMaxOutputBlockSize2);
	else
	{
		if (iOutputBlockSize2 != 0)
			OutputBuffer2.Init(iOutputBlockSize2);
	}

    for(i=0; i<veciMaxOutputBlockSize.size(); i++)
    {
		if (veciMaxOutputBlockSize[i] != 0)
			vecOutputBuffer[i].Init(veciMaxOutputBlockSize[i]);
		else
		{
			if (veciOutputBlockSize[i] != 0)
				vecOutputBuffer[i].Init(veciOutputBlockSize[i]);
		}
    }
}

template<class TInput, class TOutput> void
CReceiverModul<TInput, TOutput>::Init(CParameter& Parameter,
					std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer)
{
	size_t i;
	/* Init some internal variables */
	veciMaxOutputBlockSize.resize(vecOutputBuffer.size());
    for(i=0; i<veciMaxOutputBlockSize.size(); i++)
		veciMaxOutputBlockSize[i]=0;
	veciOutputBlockSize.resize(vecOutputBuffer.size());
    for(i=0; i<veciOutputBlockSize.size(); i++)
		veciOutputBlockSize[i]=0;
	vecbResetBuf.resize(vecOutputBuffer.size());
    for(i=0; i<vecbResetBuf.size(); i++)
		vecbResetBuf[i]=false;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter);

	/* Init output transfer buffers */
    for(i=0; i<veciMaxOutputBlockSize.size(); i++)
    {
		if (veciMaxOutputBlockSize[i] != 0)
			vecOutputBuffer[i].Init(veciMaxOutputBlockSize[i]);
		else
		{
			if (veciOutputBlockSize[i] != 0)
				vecOutputBuffer[i].Init(veciOutputBlockSize[i]);
		}
    }
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TOutput>& OutputBuffer)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
        if (trigger) {
            printf("PD-INIT\n"); //fflush(stdout);
        }
        
		/* Call init routine */
		Init(Parameter, OutputBuffer);

		/* Reset init flag */
		bDoInit = false;
	}

	/* Special case if input block size is zero */
	if (this->iInputBlockSize == 0)
	{
        //if (id) printf("PD-CLR "); fflush(stdout);
		InputBuffer.Clear();
		return false;
	}

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* Check if enough data is available in the input buffer for processing */
    //if (id) printf("PD:%d:%d ", InputBuffer.GetFillLevel(), this->iInputBlockSize); fflush(stdout);
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

		/* Copy extended data from std::vectors */
		(*(this->pvecOutputData)).
			SetExData((*(this->pvecInputData)).GetExData());

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);

		/* Reset output-buffers if flag was set by processing routine */
		if (bResetBuf)
		{
            //if (id) printf("PD-RESET-BUF "); fflush(stdout);
			/* Reset flag and clear buffer */
			bResetBuf = false;
			OutputBuffer.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer.Put(this->iOutputBlockSize);
		}
	}

    //if (id) printf("%s ", bEnoughData? "PD-TRUE":""); fflush(stdout);
	return bEnoughData;
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TOutput>& OutputBuffer,
				CBuffer<TOutput>& OutputBuffer2)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
		/* Call init routine */
		Init(Parameter, OutputBuffer, OutputBuffer2);

		/* Reset init flag */
		bDoInit = false;
	}

	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* Check if enough data is available in the input buffer for processing */
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();
		pvecOutputData2 = OutputBuffer2.QueryWriteBuffer();

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);

		/* Reset output-buffers if flag was set by processing routine */
		if (bResetBuf)
		{
			/* Reset flag and clear buffer */
			bResetBuf = false;
			OutputBuffer.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer.Put(this->iOutputBlockSize);
		}

		if (bResetBuf2)
		{
			/* Reset flag and clear buffer */
			bResetBuf2 = false;
			OutputBuffer2.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer2.Put(iOutputBlockSize2);
		}
	}

	return bEnoughData;
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TOutput>& OutputBuffer,
				CBuffer<TOutput>& OutputBuffer2,
				CBuffer<TOutput>& OutputBuffer3)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
		/* Call init routine */
		Init(Parameter, OutputBuffer, OutputBuffer2, OutputBuffer3);

		/* Reset init flag */
		bDoInit = false;
	}

	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* Check if enough data is available in the input buffer for processing */
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();
		pvecOutputData2 = OutputBuffer2.QueryWriteBuffer();
		pvecOutputData3 = OutputBuffer3.QueryWriteBuffer();

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);

		/* Reset output-buffers if flag was set by processing routine */
		if (bResetBuf)
		{
			/* Reset flag and clear buffer */
			bResetBuf = false;
			OutputBuffer.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer.Put(this->iOutputBlockSize);
		}

		if (bResetBuf2)
		{
			/* Reset flag and clear buffer */
			bResetBuf2 = false;
			OutputBuffer2.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer2.Put(iOutputBlockSize2);
		}

		if (bResetBuf3)
		{
			/* Reset flag and clear buffer */
			bResetBuf3 = false;
			OutputBuffer3.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer3.Put(iOutputBlockSize3);
		}
	}

	return bEnoughData;
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				CBuffer<TOutput>& OutputBuffer,
				CBuffer<TOutput>& OutputBuffer2,
				std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
		/* Call init routine */
		Init(Parameter, OutputBuffer, OutputBuffer2, vecOutputBuffer);

		/* Reset init flag */
		bDoInit = false;
	}

	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* Check if enough data is available in the input buffer for processing */
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		size_t i;
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();
		pvecOutputData2 = OutputBuffer2.QueryWriteBuffer();
		vecpvecOutputData.resize(vecOutputBuffer.size());
		for(i=0; i<vecOutputBuffer.size(); i++)
		{
			vecpvecOutputData[i] = vecOutputBuffer[i].QueryWriteBuffer();
		}

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);

		/* Reset output-buffers if flag was set by processing routine */
		if (bResetBuf)
		{
			/* Reset flag and clear buffer */
			bResetBuf = false;
			OutputBuffer.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer.Put(this->iOutputBlockSize);
		}

		if (bResetBuf2)
		{
			/* Reset flag and clear buffer */
			bResetBuf2 = false;
			OutputBuffer2.Clear();
		}
		else
		{
			/* Write processed data from internal memory in transfer-buffer */
			OutputBuffer2.Put(iOutputBlockSize2);
		}

		for(i=0; i<vecOutputBuffer.size(); i++)
		{
			if (vecbResetBuf[i])
			{
				/* Reset flag and clear buffer */
				vecbResetBuf[i] = false;
				vecOutputBuffer[i].Clear();
			}
			else
			{
				/* Write processed data from internal memory in transfer-buffer */
				vecOutputBuffer[i].Put(veciOutputBlockSize[i]);
			}
		}
	}

	return bEnoughData;
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	ProcessData(CParameter& Parameter, CBuffer<TInput>& InputBuffer,
				std::vector< CSingleBuffer<TOutput> >& vecOutputBuffer)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
		/* Call init routine */
		Init(Parameter, vecOutputBuffer);

		/* Reset init flag */
		bDoInit = false;
	}

	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* Check if enough data is available in the input buffer for processing */
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		size_t i;
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		vecpvecOutputData.resize(vecOutputBuffer.size());
		for(i=0; i<vecOutputBuffer.size(); i++)
		{
			vecpvecOutputData[i] = vecOutputBuffer[i].QueryWriteBuffer();
		}

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);

		/* Reset output-buffers if flag was set by processing routine */
		for(i=0; i<vecOutputBuffer.size(); i++)
		{
			if (vecbResetBuf[i])
			{
				/* Reset flag and clear buffer */
				vecbResetBuf[i] = false;
				vecOutputBuffer[i].Clear();
			}
			else
			{
				/* Write processed data from internal memory in transfer-buffer */
				vecOutputBuffer[i].Put(veciOutputBlockSize[i]);
			}
		}
	}

	return bEnoughData;
}

template<class TInput, class TOutput>
void CReceiverModul<TInput, TOutput>::
	ReadData(CParameter& Parameter, CBuffer<TOutput>& OutputBuffer)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
        if (trigger) {
            printf("Modul-INIT\n"); //fflush(stdout);
        }
        
		/* Call init routine */
		Init(Parameter, OutputBuffer);

		/* Reset init flag */
		bDoInit = false;
	}

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* Query std::vector from output transfer-buffer for writing */
	this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

	/* Call the underlying processing-routine */
	this->ProcessDataThreadSave(Parameter);

	/* Reset output-buffers if flag was set by processing routine */
	if (bResetBuf)
	{
		/* Reset flag and clear buffer */
		bResetBuf = false;
		OutputBuffer.Clear();
	}
	else
	{
		/* Write processed data from internal memory in transfer-buffer */
		OutputBuffer.Put(this->iOutputBlockSize);
	}
}

template<class TInput, class TOutput>
bool CReceiverModul<TInput, TOutput>::
	WriteData(CParameter& Parameter, CBuffer<TInput>& InputBuffer)
{
	/* Check initialization flag. The initialization must be done OUTSIDE
	   the processing routine. This is ensured by doing it here, where we
	   have control of calling the processing routine. Therefore we
	   introduced the flag */
	if (bDoInit)
	{
        //if (id) printf("WD-INIT "); fflush(stdout);
		/* Call init routine */
		Init(Parameter);

		/* Reset init flag */
		bDoInit = false;
	}

	/* Special case if input block size is zero and buffer, too */
	if ((InputBuffer.GetFillLevel() == 0) && (this->iInputBlockSize == 0))
	{
        //if (id) printf("WD-CLR "); fflush(stdout);
		InputBuffer.Clear();
		return false;
	}

	/* INPUT-DRIVEN modul implementation in the receiver -------------------- */
	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* Check if enough data is available in the input buffer for processing */
    //if (id) printf("WD:%d:%d ", InputBuffer.GetFillLevel(), this->iInputBlockSize); fflush(stdout);
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Call the underlying processing-routine */
		this->ProcessDataThreadSave(Parameter);
	}

    //if (id) printf("%s ", bEnoughData? "WD-TRUE":""); fflush(stdout);
	return bEnoughData;
}


/******************************************************************************\
* Simulation modul (CSimulationModul)                                          *
\******************************************************************************/
template<class TInput, class TOutput, class TInOut2>
CSimulationModul<TInput, TOutput, TInOut2>::CSimulationModul()
{
	/* Initialize all member variables with zeros */
	iMaxOutputBlockSize2 = 0;
	iOutputBlockSize2 = 0;
	iInputBlockSize2 = 0;
	pvecOutputData2 = nullptr;
	pvecInputData2 = nullptr;
}

template<class TInput, class TOutput, class TInOut2>
void CSimulationModul<TInput, TOutput, TInOut2>::Init(CParameter& Parameter)
{
	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter);
}

template<class TInput, class TOutput, class TInOut2>
void CSimulationModul<TInput, TOutput, TInOut2>::
	Init(CParameter& Parameter,
		 CBuffer<TOutput>& OutputBuffer)
{
	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);
}

template<class TInput, class TOutput, class TInOut2>
void CSimulationModul<TInput, TOutput, TInOut2>::
	Init(CParameter& Parameter,
		 CBuffer<TOutput>& OutputBuffer,
		 CBuffer<TInOut2>& OutputBuffer2)
{
	/* Init some internal variables */
	iMaxOutputBlockSize2 = 0;
	iOutputBlockSize2 = 0;

	/* Init base-class */
	CModul<TInput, TOutput>::Init(Parameter, OutputBuffer);

	/* Init output transfer buffers */
	if (iMaxOutputBlockSize2 != 0)
		OutputBuffer2.Init(iMaxOutputBlockSize2);
	else
	{
		if (iOutputBlockSize2 != 0)
			OutputBuffer2.Init(iOutputBlockSize2);
	}
}

template<class TInput, class TOutput, class TInOut2>
void CSimulationModul<TInput, TOutput, TInOut2>::
	TransferData(CParameter& Parameter,
				 CBuffer<TInput>& InputBuffer,
				 CBuffer<TOutput>& OutputBuffer)
{
	/* TransferData needed for simulation */
	/* Check, if enough input data is available */
	if (InputBuffer.GetFillLevel() < this->iInputBlockSize)
	{
		/* Set request flag */
		InputBuffer.SetRequestFlag(true);

		return;
	}

	/* Get std::vector from transfer-buffer */
	this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

	/* Query std::vector from output transfer-buffer for writing */
	this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

	/* Call the underlying processing-routine */
	this->ProcessDataInternal(Parameter);

	/* Write processed data from internal memory in transfer-buffer */
	OutputBuffer.Put(this->iOutputBlockSize);
}

template<class TInput, class TOutput, class TInOut2>
bool CSimulationModul<TInput, TOutput, TInOut2>::
	ProcessDataIn(CParameter& Parameter,
				  CBuffer<TInput>& InputBuffer,
				  CBuffer<TInOut2>& InputBuffer2,
				  CBuffer<TOutput>& OutputBuffer)
{
	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* Check if enough data is available in the input buffer for processing */
	if ((InputBuffer.GetFillLevel() >= this->iInputBlockSize) &&
		(InputBuffer2.GetFillLevel() >= iInputBlockSize2))
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);
		pvecInputData2 = InputBuffer2.Get(iInputBlockSize2);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();

		/* Copy extended data from FIRST input std::vector (definition!) */
		(*(this->pvecOutputData)).
			SetExData((*(this->pvecInputData)).GetExData());

		/* Call the underlying processing-routine */
		this->ProcessDataInternal(Parameter);

		/* Write processed data from internal memory in transfer-buffer */
		OutputBuffer.Put(this->iOutputBlockSize);
	}

	return bEnoughData;
}

template<class TInput, class TOutput, class TInOut2>
bool CSimulationModul<TInput, TOutput, TInOut2>::
	ProcessDataOut(CParameter& Parameter,
				   CBuffer<TInput>& InputBuffer,
				   CBuffer<TOutput>& OutputBuffer,
				   CBuffer<TInOut2>& OutputBuffer2)
{
	/* This flag shows, if enough data was in the input buffer for processing */
	bool bEnoughData = false;

	/* Check if enough data is available in the input buffer for processing */
	if (InputBuffer.GetFillLevel() >= this->iInputBlockSize)
	{
		bEnoughData = true;

		/* Get std::vector from transfer-buffer */
		this->pvecInputData = InputBuffer.Get(this->iInputBlockSize);

		/* Query std::vector from output transfer-buffer for writing */
		this->pvecOutputData = OutputBuffer.QueryWriteBuffer();
		pvecOutputData2 = OutputBuffer2.QueryWriteBuffer();

		/* Call the underlying processing-routine */
		this->ProcessDataInternal(Parameter);

		/* Write processed data from internal memory in transfer-buffers */
		OutputBuffer.Put(this->iOutputBlockSize);
		OutputBuffer2.Put(iOutputBlockSize2);
	}

	return bEnoughData;
}

/* Take an input buffer and split it 2 ways */

template<class TInput>
class CSplitModul: public CReceiverModul<TInput, TInput>
{
protected:
	virtual void SetInputBlockSize(CParameter& Parameters) = 0;

	virtual void InitInternal(CParameter& Parameters)
	{
		this->SetInputBlockSize(Parameters);
		this->iOutputBlockSize = this->iInputBlockSize;
		this->iOutputBlockSize2 = this->iInputBlockSize;
	}

	virtual void ProcessDataInternal(CParameter&)
	{
		for (int i = 0; i < this->iInputBlockSize; i++)
		{
			TInput n = (*(this->pvecInputData))[i];
			(*this->pvecOutputData)[i] = n;
			(*this->pvecOutputData2)[i] = n;
		}
	}
};

#endif // !defined(AFX_MODUL_H__41E39CD3_2AEC_400E_907B_148C0EC17A43__INCLUDED_)
