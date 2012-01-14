/*
 *      Copyright (C) 2011 Hendrik Leppkes
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
 *
 *  Basic concept based on VLC DXVA2 decoder, licensed under GPLv2
 */

#include "stdafx.h"
#include "dxva2dec.h"
#include "moreuuids.h"
#include "Media.h"

#include <Shlwapi.h>

#include <dxva2api.h>
#include "libavcodec/dxva2.h"

#include "gpu_memcpy_sse4.h"

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

ILAVDecoder *CreateDecoderDXVA2() {
  return new CDecDXVA2();
}

////////////////////////////////////////////////////////////////////////////////
// Codec Maps
////////////////////////////////////////////////////////////////////////////////

/*
DXVA2 Codec Mappings, as defined by VLC
*/
typedef struct {
  const char   *name;
  const GUID   *guid;
  int          codec;
} dxva2_mode_t;
/* XXX Prefered modes must come first */
static const dxva2_mode_t dxva2_modes[] = {
  /* Intel specific modes (sometimes work better on newer GPUs) */
  { "H.264 variable-length decoder, no film grain technology (Intel ClearVideo)",   &DXVADDI_Intel_ModeH264_E,              CODEC_ID_H264 },
  { "H.264 inverse discrete cosine transform, no film grain technology (Intel)",    &DXVADDI_Intel_ModeH264_C,              0 },
  { "H.264 motion compensation, no film grain technology (Intel)",                  &DXVADDI_Intel_ModeH264_A,              0 },
  { "VC-1 variable-length decoder 2 (Intel)",                                       &DXVA_Intel_VC1_ClearVideo_2,           0 },
  { "VC-1 variable-length decoder (Intel)",                                         &DXVA_Intel_VC1_ClearVideo,             0 },

  /* MPEG-1/2 */
  { "MPEG-2 variable-length decoder",                                               &DXVA2_ModeMPEG2_VLD,                   CODEC_ID_MPEG2VIDEO },
  { "MPEG-2 & MPEG-1 variable-length decoder",                                      &DXVA2_ModeMPEG2and1_VLD,               CODEC_ID_MPEG2VIDEO },
  { "MPEG-2 motion compensation",                                                   &DXVA2_ModeMPEG2_MoComp,                0 },
  { "MPEG-2 inverse discrete cosine transform",                                     &DXVA2_ModeMPEG2_IDCT,                  0 },

  { "MPEG-1 variable-length decoder",                                               &DXVA2_ModeMPEG1_VLD,                   0 },

  /* H.264 */
  { "H.264 variable-length decoder, film grain technology",                         &DXVA2_ModeH264_F,                      CODEC_ID_H264 },
  { "H.264 variable-length decoder, no film grain technology",                      &DXVA2_ModeH264_E,                      CODEC_ID_H264 },
  { "H.264 variable-length decoder, no film grain technology, FMO/ASO",             &DXVA_ModeH264_VLD_WithFMOASO_NoFGT,    CODEC_ID_H264 },
  { "H.264 variable-length decoder, no film grain technology, Flash",               &DXVA_ModeH264_VLD_NoFGT_Flash,         CODEC_ID_H264 },

  { "H.264 inverse discrete cosine transform, film grain technology",               &DXVA2_ModeH264_D,                      0 },
  { "H.264 inverse discrete cosine transform, no film grain technology",            &DXVA2_ModeH264_C,                      0 },

  { "H.264 motion compensation, film grain technology",                             &DXVA2_ModeH264_B,                      0 },
  { "H.264 motion compensation, no film grain technology",                          &DXVA2_ModeH264_A,                      0 },

  /* WMV */
  { "Windows Media Video 8 motion compensation",                                    &DXVA2_ModeWMV8_B,                      0 },
  { "Windows Media Video 8 post processing",                                        &DXVA2_ModeWMV8_A,                      0 },

  { "Windows Media Video 9 IDCT",                                                   &DXVA2_ModeWMV9_C,                      0 },
  { "Windows Media Video 9 motion compensation",                                    &DXVA2_ModeWMV9_B,                      0 },
  { "Windows Media Video 9 post processing",                                        &DXVA2_ModeWMV9_A,                      0 },

  /* VC-1 */
  { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       CODEC_ID_VC1 },
  { "VC-1 variable-length decoder",                                                 &DXVA2_ModeVC1_D,                       CODEC_ID_WMV3 },
  { "VC-1 variable-length decoder (2010)",                                          &DXVA2_ModeVC1_D2010,                   CODEC_ID_VC1 },
  { "VC-1 variable-length decoder (2010)",                                          &DXVA2_ModeVC1_D2010,                   CODEC_ID_WMV3 },

  { "VC-1 inverse discrete cosine transform",                                       &DXVA2_ModeVC1_C,                       0 },
  { "VC-1 motion compensation",                                                     &DXVA2_ModeVC1_B,                       0 },
  { "VC-1 post processing",                                                         &DXVA2_ModeVC1_A,                       0 },

  /* MPEG4-ASP */
  { "MPEG-4 Part 2 nVidia bitstream decoder",                                       &DXVA_nVidia_MPEG4_ASP,                 0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple Profile",                        &DXVA_ModeMPEG4pt2_VLD_Simple,          0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, no GMC",       &DXVA_ModeMPEG4pt2_VLD_AdvSimple_NoGMC, 0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, GMC",          &DXVA_ModeMPEG4pt2_VLD_AdvSimple_GMC,   0 },
  { "MPEG-4 Part 2 variable-length decoder, Simple&Advanced Profile, Avivo",        &DXVA_ModeMPEG4pt2_VLD_AdvSimple_Avivo, 0 },

  /* H.264 MVC */
  { "H.264 MVC variable-length decoder, stereo, progressive",                       &DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT, 0 },
  { "H.264 MVC variable-length decoder, stereo",                                    &DXVA_ModeH264_VLD_Stereo_NoFGT,             0 },
  { "H.264 MVC variable-length decoder, multiview",                                 &DXVA_ModeH264_VLD_Multiview_NoFGT,          0 },

  { NULL, NULL, 0 }
};

static const dxva2_mode_t *DXVA2FindMode(const GUID *guid)
{
  for (unsigned i = 0; dxva2_modes[i].name; i++) {
    if (IsEqualGUID(*dxva2_modes[i].guid, *guid))
      return &dxva2_modes[i];
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DXVA2 decoder implementation
////////////////////////////////////////////////////////////////////////////////

static void (*CopyFrameNV12)(const BYTE *pSourceData, BYTE *pY, BYTE *pUV, int srcLines, int dstLines, int pitch) = NULL;

static void CopyFrameNV12_fallback(const BYTE *pSourceData, BYTE *pY, BYTE *pUV, int srcLines, int dstLines, int pitch)
{
  const int size = dstLines * pitch;
  memcpy(pY, pSourceData, size);
  memcpy(pUV, pSourceData + (srcLines * pitch), size >> 1);
}

static void CopyFrameNV12_SSE4(const BYTE *pSourceData, BYTE *pY, BYTE *pUV, int srcLines, int dstLines, int pitch)
{
  const int size = dstLines * pitch;
  gpu_memcpy(pY, pSourceData, size);
  gpu_memcpy(pUV, pSourceData + (srcLines * pitch), size >> 1);
}

CDecDXVA2::CDecDXVA2(void)
  : CDecAvcodec()
  , m_pD3D(NULL)
  , m_pD3DDev(NULL)
  , m_pD3DDevMngr(NULL)
  , m_pD3DResetToken(0)
  , m_pDXVADecoderService(NULL)
  , m_pDecoder(NULL)
  , m_NumSurfaces(0)
  , m_CurrentSurfaceAge(1)
  , m_FrameQueuePosition(0)
  , m_bFailHWDecode(FALSE)
  , m_dwSurfaceWidth(0)
  , m_dwSurfaceHeight(0)
  , m_dwVendorId(0)
{
  ZeroMemory(&dx, sizeof(dx));
  ZeroMemory(&m_DXVAExtendedFormat, sizeof(m_DXVAExtendedFormat));
  ZeroMemory(&m_pSurfaces, sizeof(m_pSurfaces));
  ZeroMemory(&m_FrameQueue, sizeof(m_FrameQueue));
}

CDecDXVA2::~CDecDXVA2(void)
{
  DestroyDecoder(true);
}

STDMETHODIMP CDecDXVA2::DestroyDecoder(bool bFull, bool bNoAVCodec)
{
  SafeRelease(&m_pDecoder);

  for (int i = 0; i < m_NumSurfaces; i++) {
    SafeRelease(&m_pSurfaces[i].d3d);
  }
  m_NumSurfaces = 0;

  for (int i = 0; i < DXVA2_QUEUE_SURFACES; i++) {
    SAFE_CO_FREE(m_FrameQueue[i]);
  }

  if (!bNoAVCodec) {
    if (m_pAVCtx) {
      av_freep(&m_pAVCtx->hwaccel_context);
    }

    CDecAvcodec::DestroyDecoder();
  }

  if (bFull) {
    SafeRelease(&m_pDXVADecoderService);
    SafeRelease(&m_pD3DDevMngr);
    SafeRelease(&m_pD3DDev);
    SafeRelease(&m_pD3D);

    if (dx.dxva2lib)
      FreeLibrary(dx.dxva2lib);
  }

  return S_OK;
}

STDMETHODIMP CDecDXVA2::LoadDXVA2Functions()
{
  // Load DXVA2 library
  dx.dxva2lib = LoadLibrary(L"dxva2.dll");
  if (dx.dxva2lib == NULL) {
    DbgLog((LOG_TRACE, 10, L"-> Loading dxva2.dll failed"));
    return E_FAIL;
  }

  dx.createDeviceManager = (pCreateDeviceManager9 *)GetProcAddress(dx.dxva2lib, "DXVA2CreateDirect3DDeviceManager9");
  if (dx.createDeviceManager == NULL) {
    DbgLog((LOG_TRACE, 10, L"-> DXVA2CreateDirect3DDeviceManager9 unavailable"));
    return E_FAIL;
  }

  return S_OK;
}

static const struct {
  unsigned id;
  char     name[32];
} vendors [] = {
  { 0x1002, "ATI" },
  { 0x10DE, "NVIDIA" },
  { 0x8086, "Intel" },
  { 0, "" }
};

HRESULT CDecDXVA2::CreateD3DDeviceManager(IDirect3DDevice9 *pDevice, UINT *pReset, IDirect3DDeviceManager9 **ppManager)
{
  UINT resetToken = 0;
  IDirect3DDeviceManager9 *pD3DManager = NULL;
  HRESULT hr = dx.createDeviceManager(&resetToken, &pD3DManager);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> DXVA2CreateDirect3DDeviceManager9 failed"));
    goto done;
  }

  hr = pD3DManager->ResetDevice(pDevice, resetToken);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> ResetDevice failed"));
    goto done;
  }

  *ppManager = pD3DManager;
  (*ppManager)->AddRef();

  *pReset = resetToken;
done:
  SafeRelease(&pD3DManager);
  return hr;
}

HRESULT CDecDXVA2::CreateDXVAVideoService(IDirect3DDevice9 *pDevice, IDirect3DDeviceManager9 *pManager, IDirectXVideoDecoderService **ppService)
{
  HRESULT hr = S_OK;

  IDirectXVideoDecoderService *pService;
  HANDLE hDevice;

  hr = pManager->OpenDeviceHandle(&hDevice);
  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> OpenDeviceHandle failed"));
    goto done;
  }

  HRESULT hr2 = pManager->GetVideoService(hDevice, IID_IDirectXVideoDecoderService, (void**)&pService);

  // Close the device handle.
  hr = pManager->CloseDeviceHandle(hDevice);

  if (FAILED(hr2)) {
    DbgLog((LOG_ERROR, 10, L"-> Acquiring VideoDecoderService failed"));
    goto done;
  }

  if (FAILED(hr)) {
    DbgLog((LOG_ERROR, 10, L"-> CloseDeviceHandle failed"));
    goto done;
  }

  (*ppService) = pService;
  (*ppService)->AddRef();

done:
  SafeRelease(&pService);
  return hr;
}

HRESULT CDecDXVA2::FindVideoServiceConversion(CodecID codec, GUID *input, D3DFORMAT *output)
{
  HRESULT hr = S_OK;

  UINT count = 0;
  GUID *input_list = NULL;

  /* Gather the format supported by the decoder */
  hr = m_pDXVADecoderService->GetDecoderDeviceGuids(&count, &input_list);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> GetDecoderDeviceGuids failed with hr: %X", hr));
    goto done;
  }

  DbgLog((LOG_TRACE, 10, L"-> Enumerating supported DXVA2 modes (count: %d)", count));
  for(unsigned i = 0; i < count; i++) {
    const GUID *g = &input_list[i];
    const dxva2_mode_t *mode = DXVA2FindMode(g);
    if (mode) {
      DbgLog((LOG_TRACE, 10, L"  -> %S", mode->name));
    } else {
      DbgLog((LOG_TRACE, 10, L"  -> Unknown GUID (%s)", WStringFromGUID(*g).c_str()));
    }
  }

  /* Iterate over our priority list */
  for (unsigned i = 0; dxva2_modes[i].name; i++) {
    const dxva2_mode_t *mode = &dxva2_modes[i];
     if (!mode->codec || mode->codec != codec)
       continue;

     BOOL supported = FALSE;
     for (const GUID *g = &input_list[0]; !supported && g < &input_list[count]; g++) {
       supported = IsEqualGUID(*mode->guid, *g);
     }
     if (!supported)
       continue;

     DbgLog((LOG_TRACE, 10, L"-> Trying to use '%S'", mode->name));
     UINT out_count = 0;
     D3DFORMAT *out_list = NULL;
     hr = m_pDXVADecoderService->GetDecoderRenderTargets(*mode->guid, &out_count, &out_list);
     if (FAILED(hr)) {
       DbgLog((LOG_TRACE, 10, L"-> Retrieving render targets failed with hr: %X", hr));
       continue;
     }

     BOOL hasNV12 = FALSE;
     DbgLog((LOG_TRACE, 10, L"-> Enumerating render targets (count: %d)", out_count));
     for (unsigned j = 0; j < out_count; j++) {
       const D3DFORMAT f = out_list[j];
       DbgLog((LOG_TRACE, 10, L"  -> %d is supported (%4.4S)", f, (const char *)&f));
       if (f == FOURCC_NV12) {
         hasNV12 = TRUE;
       }
     }
     if (hasNV12) {
       DbgLog((LOG_TRACE, 10, L"-> Found NV12, finished setup"));
       *input = *mode->guid;
       *output = (D3DFORMAT)FOURCC_NV12;

       SAFE_CO_FREE(out_list);
       SAFE_CO_FREE(input_list);
       return S_OK;
     }

     SAFE_CO_FREE(out_list);
  }

done:
  SAFE_CO_FREE(input_list);
  return E_FAIL;
}

// ILAVDecoder
STDMETHODIMP CDecDXVA2::Init()
{
  DbgLog((LOG_TRACE, 10, L"CDecDXVA2::Init(): Trying to open DXVA2 decoder"));
  HRESULT hr = S_OK;

  if (FAILED(hr = LoadDXVA2Functions())) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to load DXVA2 DLL functions"));
    return E_FAIL;
  }

  m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  if (!m_pD3D) {
    DbgLog((LOG_ERROR, 10, L"-> Failed to acquire IDirect3D9"));
    return E_FAIL;
  }

  int lAdapter = D3DADAPTER_DEFAULT;

  D3DADAPTER_IDENTIFIER9 d3dai = {0};
  m_pD3D->GetAdapterIdentifier(lAdapter, 0, &d3dai);

  const char *vendor = "Unknown";
  for (int i = 0; vendors[i].id != 0; i++) {
    if (vendors[i].id == d3dai.VendorId) {
      vendor = vendors[i].name;
      break;
    }
  }

  DbgLog((LOG_TRACE, 10, L"-> Running on adapter %d, %S, vendor 0x%04X(%S), device 0x%04X", lAdapter, d3dai.Description, d3dai.VendorId, vendor, d3dai.DeviceId));
  m_dwVendorId = d3dai.VendorId;

  D3DPRESENT_PARAMETERS d3dpp;
  D3DDISPLAYMODE d3ddm;

  ZeroMemory(&d3dpp, sizeof(d3dpp));
  m_pD3D->GetAdapterDisplayMode(lAdapter, &d3ddm);

  d3dpp.Windowed               = TRUE;
  d3dpp.BackBufferWidth        = 640;
  d3dpp.BackBufferHeight       = 480;
  d3dpp.BackBufferCount        = 0;
  d3dpp.BackBufferFormat       = d3ddm.Format;
  d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
  d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;

  hr = m_pD3D->CreateDevice(lAdapter, D3DDEVTYPE_HAL, GetShellWindow(), D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &d3dpp, &m_pD3DDev);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Creation of device failed with hr: %X", hr));
    return E_FAIL;
  }

  hr = CreateD3DDeviceManager(m_pD3DDev, &m_pD3DResetToken, &m_pD3DDevMngr);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Creation of Device manager failed with hr: %X", hr));
    return E_FAIL;
  }

  hr = CreateDXVAVideoService(m_pD3DDev, m_pD3DDevMngr, &m_pDXVADecoderService);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Creation of DXVA2 Decoder Service failed with hr: %X", hr));
    return E_FAIL;
  }

  // Init the ffmpeg parts
  // This is our main software decoder, unable to fail!
  CDecAvcodec::Init();

  if (CopyFrameNV12 == NULL) {
    int cpu_flags = av_get_cpu_flags();
    if (cpu_flags & AV_CPU_FLAG_SSE4) {
      DbgLog((LOG_TRACE, 10, L"-> Using SSE4 frame copy"));
      CopyFrameNV12 = CopyFrameNV12_SSE4;
    } else if (FALSE && cpu_flags & AV_CPU_FLAG_SSE2) {
      DbgLog((LOG_TRACE, 10, L"-> Using SSE2 frame copy"));
      //CopyFrameNV12 = CopyFrameNV12_SSE2;
    } else {
      DbgLog((LOG_TRACE, 10, L"-> Using fallback frame copy"));
      CopyFrameNV12 = CopyFrameNV12_fallback;
    }
  }

  return S_OK;
}

STDMETHODIMP CDecDXVA2::InitDecoder(CodecID codec, const CMediaType *pmt)
{
  HRESULT hr = S_OK;
  DbgLog((LOG_TRACE, 10, L"CDecDXVA2::InitDecoder(): Initializing DXVA2 decoder"));

  DestroyDecoder(false);

  GUID input = GUID_NULL;
  D3DFORMAT output;
  hr = FindVideoServiceConversion(codec, &input, &output);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> No decoder device available that can decode codec '%S' to NV12", avcodec_get_name(codec)));
    return E_FAIL;
  }

  m_bFailHWDecode = FALSE;

  DbgLog((LOG_TRACE, 10, L"-> Creation of DXVA2 decoder successfull, initializing ffmpeg"));
  hr = CDecAvcodec::InitDecoder(codec, pmt);
  if (FAILED(hr)) {
    return hr;
  }

  if ((codec == CODEC_ID_H264 || codec == CODEC_ID_MPEG2VIDEO) && m_pAVCtx->pix_fmt != PIX_FMT_YUV420P && m_pAVCtx->pix_fmt != PIX_FMT_DXVA2_VLD && m_pAVCtx->pix_fmt != PIX_FMT_NONE) {
    DbgLog((LOG_TRACE, 10, L"-> Incompatible pixel format detected, falling back to software decoding"));
    return E_FAIL;
  }

  return S_OK;
}

HRESULT CDecDXVA2::CreateDXVA2Decoder()
{
  HRESULT hr = S_OK;

  DestroyDecoder(false, true);

  GUID input = GUID_NULL;
  D3DFORMAT output;
  FindVideoServiceConversion(m_pAVCtx->codec_id, &input, &output);

  m_dwSurfaceWidth = FFALIGN(m_pAVCtx->coded_width, 16);
  m_dwSurfaceHeight = FFALIGN(m_pAVCtx->coded_height, 16);

  m_NumSurfaces = (m_pAVCtx->codec_id == CODEC_ID_H264) ? 16 + DXVA2_QUEUE_SURFACES + 2 : 2 + DXVA2_QUEUE_SURFACES + 2;
  LPDIRECT3DSURFACE9 pSurfaces[DXVA2_MAX_SURFACES];
  hr = m_pDXVADecoderService->CreateSurface(m_dwSurfaceWidth, m_dwSurfaceHeight, m_NumSurfaces - 1, output, D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, pSurfaces, NULL);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> Creation of surfaces failed with hr: %X", hr));
    m_NumSurfaces = 0;
    return E_FAIL;
  }

  for (int i = 0; i < m_NumSurfaces; i++) {
    m_pSurfaces[i].d3d = pSurfaces[i];
    m_pSurfaces[i].ref = 0;
    m_pSurfaces[i].age = 0;
  }

  DbgLog((LOG_TRACE, 10, L"-> Successfully created %d surfaces (%dx%d)", m_NumSurfaces, m_dwSurfaceWidth, m_dwSurfaceHeight));

  DXVA2_VideoDesc desc;
  ZeroMemory(&desc, sizeof(desc));
  desc.SampleWidth = m_pAVCtx->width;
  desc.SampleHeight = m_pAVCtx->height;
  desc.Format = output;

  UINT cfg_count = 0;
  DXVA2_ConfigPictureDecode *cfg_list = NULL;
  hr = m_pDXVADecoderService->GetDecoderConfigurations(input, &desc, NULL, &cfg_count, &cfg_list);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> GetDecoderConfigurations failed with hr: %X", hr));
    return E_FAIL;
  }

  DbgLog((LOG_TRACE, 10, L"-> We got %d decoder configurations", cfg_count));
  int best_score = 0;
  DXVA2_ConfigPictureDecode best_cfg;
  for (unsigned i = 0; i < cfg_count; i++) {
    DXVA2_ConfigPictureDecode *cfg = &cfg_list[i];

    int score;
    if (cfg->ConfigBitstreamRaw == 1)
      score = 1;
    else if (m_pAVCtx->codec_id == CODEC_ID_H264 && cfg->ConfigBitstreamRaw == 2)
      score = 2;
    else
      continue;
    if (IsEqualGUID(cfg->guidConfigBitstreamEncryption, DXVA2_NoEncrypt))
      score += 16;
    if (score > best_score) {
      best_score = score;
      best_cfg = *cfg;
    }
  }
  SAFE_CO_FREE(cfg_list);
  if (best_score <= 0) {
    DbgLog((LOG_TRACE, 10, L"-> No matching configuration available"));
    return E_FAIL;
  }

  m_DXVAVideoDecoderConfig = best_cfg;

  IDirectXVideoDecoder *decoder;
  hr = m_pDXVADecoderService->CreateVideoDecoder(input, &desc, &m_DXVAVideoDecoderConfig, pSurfaces, m_NumSurfaces, &decoder);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"-> CreateVideoDecoder failed with hr: %X", hr));
    return E_FAIL;
  }
  m_pDecoder = decoder;

  /* fill hwaccel_context */
  dxva_context *ctx = (dxva_context *)m_pAVCtx->hwaccel_context;
  ctx->cfg           = &m_DXVAVideoDecoderConfig;
  ctx->decoder       = m_pDecoder;
  ctx->surface       = m_pRawSurface;
  ctx->surface_count = m_NumSurfaces;

  memset(m_pRawSurface, 0, sizeof(m_pRawSurface));
  for (int i = 0; i < m_NumSurfaces; i++) {
    m_pRawSurface[i] = m_pSurfaces[i].d3d;
  }

  return S_OK;
}

static enum PixelFormat get_dxva2_format(struct AVCodecContext *s, const enum PixelFormat * fmt)
{
  while (*fmt != PIX_FMT_NONE && *fmt != PIX_FMT_DXVA2_VLD) {
    ++fmt;
  }
  return fmt[0];
}

int CDecDXVA2::get_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic)
{
  CDecDXVA2 *pDec = (CDecDXVA2 *)c->opaque;

  HRESULT hr = S_OK;

  if (!pDec->m_pDecoder || FFALIGN(c->coded_width, 16) != pDec->m_dwSurfaceWidth || FFALIGN(c->coded_height, 16) != pDec->m_dwSurfaceHeight) {
    hr = pDec->CreateDXVA2Decoder();
    if (FAILED(hr)) {
      pDec->m_bFailHWDecode = TRUE;
      return -1;
    }
  }

  if (c->pix_fmt != PIX_FMT_DXVA2_VLD) {
    DbgLog((LOG_ERROR, 10, L"DXVA2 buffer request, but not dxva2 pixfmt"));
    pDec->m_bFailHWDecode = TRUE;
    return -1;
  }

  pic->type = FF_BUFFER_TYPE_USER;
  pic->age  = 256*256*256*64; // What?

  //pDec->m_pD3DDevMngr->TestDevice(
  int i, old, old_unused;
  for (i = 0, old = 0, old_unused = -1; i < pDec->m_NumSurfaces; i++) {
    d3d_surface_t *surface = &pDec->m_pSurfaces[i];
    if (!surface->ref && (old_unused == -1 || surface->age < pDec->m_pSurfaces[old_unused].age))
      old_unused = i;
    if (surface->age < pDec->m_pSurfaces[old].age)
      old = i;
  }
  if (old_unused == -1) {
    DbgLog((LOG_TRACE, 10, L"No free surface, using oldest"));
    i = old;
  } else {
    i = old_unused;
  }

  LPDIRECT3DSURFACE9 pSurface = pDec->m_pSurfaces[i].d3d;

  pDec->m_pSurfaces[i].age = pDec->m_CurrentSurfaceAge++;
  pDec->m_pSurfaces[i].ref = 1;

  memset(pic->data, 0, sizeof(pic->data));
  memset(pic->linesize, 0, sizeof(pic->linesize));

  pic->data[0] = pic->data[3] = (uint8_t *)pSurface;

  return 0;
}

void CDecDXVA2::release_dxva2_buffer(struct AVCodecContext *c, AVFrame *pic)
{
  CDecDXVA2 *pDec = (CDecDXVA2 *)c->opaque;

  LPDIRECT3DSURFACE9 pSurface = (LPDIRECT3DSURFACE9)pic->data[3];
  for (int i = 0; i < pDec->m_NumSurfaces; i++) {
    if (pDec->m_pSurfaces[i].d3d == pSurface)
      pDec->m_pSurfaces[i].ref--;
  }

  memset(pic->data, 0, sizeof(pic->data));
}

HRESULT CDecDXVA2::AdditionaDecoderInit()
{
  /* Create ffmpeg dxva_context, but keep it empty. When this is called, we don't have the data yet */
  dxva_context *ctx = (dxva_context *)av_mallocz(sizeof(dxva_context));

  if (m_dwVendorId == 0x8086)
    ctx->workaround == FF_DXVA2_WORKAROUND_INTEL_GMA;

  m_pAVCtx->thread_count    = 1;
  m_pAVCtx->hwaccel_context = ctx;
  m_pAVCtx->get_format      = get_dxva2_format;
  m_pAVCtx->get_buffer      = get_dxva2_buffer;
  m_pAVCtx->release_buffer  = release_dxva2_buffer;
  m_pAVCtx->opaque          = this;

  return S_OK;
}

__forceinline d3d_surface_t *CDecDXVA2::FindSurface(LPDIRECT3DSURFACE9 pSurface)
{
  for (int i = 0; i < m_NumSurfaces; i++) {
    if (m_pSurfaces[i].d3d == pSurface)
      return &m_pSurfaces[i];
  }
  ASSERT(0);
  return NULL;
}

HRESULT CDecDXVA2::PostDecode()
{
  if (m_bFailHWDecode) {
    DbgLog((LOG_TRACE, 10, L"::PostDecode(): HW Decoder failed, falling back to software decoding"));
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP CDecDXVA2::Flush()
{
  CDecAvcodec::Flush();

  for (int i = 0; i < m_NumSurfaces; i++) {
    d3d_surface_t *s = &m_pSurfaces[i];
    s->ref = 0;
    //s->age = 0;
  }

  // Flush display queue
  for (int i=0; i < DXVA2_QUEUE_SURFACES; ++i) {
    if (m_FrameQueue[m_FrameQueuePosition]) {
      ReleaseFrame(&m_FrameQueue[m_FrameQueuePosition]);
    }
    m_FrameQueuePosition = (m_FrameQueuePosition + 1) % DXVA2_QUEUE_SURFACES;
  }

  return S_OK;
}

STDMETHODIMP CDecDXVA2::EndOfStream()
{
  CDecAvcodec::EndOfStream();

  // Flush display queue
  for (int i=0; i < DXVA2_QUEUE_SURFACES; ++i) {
    if (m_FrameQueue[m_FrameQueuePosition]) {
      DeliverDXVA2Frame(m_FrameQueue[m_FrameQueuePosition]);
      m_FrameQueue[m_FrameQueuePosition] = NULL;
    }
    m_FrameQueuePosition = (m_FrameQueuePosition + 1) % DXVA2_QUEUE_SURFACES;
  }

  return S_OK;
}

HRESULT CDecDXVA2::HandleDXVA2Frame(LAVFrame *pFrame)
{
  LAVFrame *pQueuedFrame = m_FrameQueue[m_FrameQueuePosition];
  m_FrameQueue[m_FrameQueuePosition] = pFrame;

  d3d_surface_t *s = FindSurface((LPDIRECT3DSURFACE9)pFrame->data[3]);
  s->ref++;

  m_FrameQueuePosition = (m_FrameQueuePosition + 1) % DXVA2_QUEUE_SURFACES;

  if (pQueuedFrame) {
    s = FindSurface((LPDIRECT3DSURFACE9)pQueuedFrame->data[3]);
    s->ref--;
    DeliverDXVA2Frame(pQueuedFrame);
  }

  return S_OK;
}

HRESULT CDecDXVA2::DeliverDXVA2Frame(LAVFrame *pFrame)
{
  if (CopyFrame(pFrame))
    Deliver(pFrame);
  else
    ReleaseFrame(&pFrame);

  return S_OK;
}

STDMETHODIMP CDecDXVA2::GetPixelFormat(LAVPixelFormat *pPix, int *pBpp)
{
  // Output is always NV12
  if (pPix)
    *pPix = LAVPixFmt_NV12;
  if (pBpp)
    *pBpp = 8;
  return S_OK;
}

__forceinline bool CDecDXVA2::CopyFrame(LAVFrame *pFrame)
{
  HRESULT hr;
  LPDIRECT3DSURFACE9 pSurface = (LPDIRECT3DSURFACE9)pFrame->data[3];
  pFrame->format = LAVPixFmt_NV12;

  D3DSURFACE_DESC surfaceDesc;
  pSurface->GetDesc(&surfaceDesc);

  D3DLOCKED_RECT LockedRect;
  hr = pSurface->LockRect(&LockedRect, NULL, D3DLOCK_READONLY);
  if (FAILED(hr)) {
    DbgLog((LOG_TRACE, 10, L"pSurface->LockRect failed (hr: %X)", hr));
    return false;
  }

  AllocLAVFrameBuffers(pFrame, LockedRect.Pitch);
  CopyFrameNV12((BYTE *)LockedRect.pBits, pFrame->data[0], pFrame->data[1], surfaceDesc.Height, pFrame->height, LockedRect.Pitch);

  pSurface->UnlockRect();

  return true;
}