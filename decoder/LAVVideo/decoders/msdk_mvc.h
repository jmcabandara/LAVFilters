/*
*      Copyright (C) 2010-2016 Hendrik Leppkes
*      http://www.1f0.de
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along
*  with this program; if not, write to the Free Software Foundation, Inc.,
*  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once
#include "DecBase.h"

#include "mfxvideo.h"
#include "mfxmvc.h"

#include "parsers/AnnexBConverter.h"

#include <vector>

#define ASYNC_DEPTH 10

typedef struct _MVCBuffer {
  mfxFrameSurface1 surface = { 0 };
  bool queued = false;
  mfxSyncPoint sync = nullptr;
} MVCBuffer;

class CDecMSDKMVC : public CDecBase
{
public:
  CDecMSDKMVC();
  virtual ~CDecMSDKMVC();

  // ILAVDecoder
  STDMETHODIMP InitDecoder(AVCodecID codec, const CMediaType *pmt);
  STDMETHODIMP Decode(const BYTE *buffer, int buflen, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, BOOL bSyncPoint, BOOL bDiscontinuity);
  STDMETHODIMP Flush();
  STDMETHODIMP EndOfStream();
  STDMETHODIMP GetPixelFormat(LAVPixelFormat *pPix, int *pBpp) { if (pPix) *pPix = LAVPixFmt_NV12; if (pBpp) *pBpp = 8; return S_OK; }
  STDMETHODIMP_(BOOL) IsInterlaced() { return FALSE; }
  STDMETHODIMP_(const WCHAR*) GetDecoderName() { return L"msdk mvc"; }
  STDMETHODIMP HasThreadSafeBuffers() { return S_OK; }

  // CDecBase
  STDMETHODIMP Init();

private:
  void DestroyDecoder(bool bFull);
  STDMETHODIMP AllocateMVCExtBuffers();

  MVCBuffer * GetBuffer();
  MVCBuffer * FindBuffer(mfxFrameSurface1 * pSurface);
  void ReleaseBuffer(mfxFrameSurface1 * pSurface);

  HRESULT HandleOutput(MVCBuffer * pOutputBuffer);
  HRESULT DeliverOutput(MVCBuffer * pBaseView, MVCBuffer * pExtraView);

  static void msdk_buffer_destruct(LAVFrame *pFrame);

private:

  mfxSession m_mfxSession = nullptr;
  mfxVersion m_mfxVersion = { 0 };

  BOOL                 m_bDecodeReady   = FALSE;
  mfxVideoParam        m_mfxVideoParams = { 0 };

  mfxExtBuffer        *m_mfxExtParam[1] = { 0 };
  mfxExtMVCSeqDesc     m_mfxExtMVCSeq   = { 0 };

  CCritSec m_BufferCritSec;
  std::vector<MVCBuffer *> m_BufferQueue;

  GrowableArray<BYTE>  m_buff;
  CAnnexBConverter    *m_pAnnexBConverter = nullptr;

  MVCBuffer           *m_pOutputQueue[ASYNC_DEPTH] = { 0 };
  int                  m_nOutputQueuePosition = 0;
};
