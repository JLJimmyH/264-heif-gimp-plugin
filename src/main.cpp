#include <string.h>
#include <assert.h>
#include "config.h"
#include "plugin-intl.h"
#include "openh264/codec_api.h"
#include "openh264/codec_app_def.h"
#include "heifreader.h"
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#define RANGE(x, low, high) (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))


using namespace HEIF;
/*  Constants  */

#define LOAD_PROC   "load_heif_file"
#define SAVE_PROC   "save_heif_file"

/*  Local function prototypes  */

static void   query(void);

static void   run(const gchar      *name,
                  gint              nparams,
                  const GimpParam  *param,
                  gint             *nreturn_vals,
                  GimpParam       **return_vals);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};


MAIN ()


static void
query (void)
{
  static const GimpParamDef load_args[] =
    {
      { GIMP_PDB_INT32,  "run-mode",     "The run mode { RUN-NONINTERACTIVE (1) }" },
      { GIMP_PDB_STRING, "filename",     "The name of the file to load" },
      { GIMP_PDB_STRING, "heif-filename","The name entered"             }
    };

  static const GimpParamDef load_return_vals[] =
    {
      { GIMP_PDB_IMAGE, "image", "Output image" }
    };

    gimp_install_procedure (LOAD_PROC,
                            _("Load HEIF images."),
                            _("Load image stored in HEIF format with 264 bitstream."),
                            "Jimmy <jimmy@jeilin.com.tw>",
                            "Jimmy <jimmy@jeilin.com.tw>",
                            "2023",
                            _("HEIF/264"),
                            NULL,
                            GIMP_PLUGIN,
                            G_N_ELEMENTS (load_args),
                            G_N_ELEMENTS (load_return_vals),
                            load_args,
                            load_return_vals);

    gimp_register_load_handler(LOAD_PROC, "heif", ""); // TODO: 'avci'
}



#define LOAD_HEIF_ERROR -1
#define LOAD_HEIF_CANCEL -2


void Write2File (FILE* pFp, unsigned char* pData[3], int iStride[2], int iWidth, int iHeight) {
  int   i;
  unsigned char*  pPtr = NULL;

  pPtr = pData[0];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[0];
  }

  iHeight = iHeight / 2;
  iWidth = iWidth / 2;
  pPtr = pData[1];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[1];
  }

  pPtr = pData[2];
  for (i = 0; i < iHeight; i++) {
    fwrite (pPtr, 1, iWidth, pFp);
    pPtr += iStride[1];
  }
}
void Write2Buf(unsigned char *pBuf, unsigned char* pData[3], int iStride[2], int iWidth, int iHeight) {
  int   i;
  unsigned char*  pPtr = NULL;

  pPtr = pData[0];
  for (i = 0; i < iHeight; i++) {
    memcpy(pBuf, pPtr, iWidth);
    pBuf += iWidth;
    pPtr += iStride[0];
  }

  iHeight = iHeight / 2;
  iWidth = iWidth / 2;
  pPtr = pData[1];
  for (i = 0; i < iHeight; i++) {
    memcpy(pBuf, pPtr, iWidth);
    pBuf += iWidth;
    pPtr += iStride[1];
  }

  pPtr = pData[2];
  for (i = 0; i < iHeight; i++) {
    memcpy(pBuf, pPtr, iWidth);
    pBuf += iWidth;
    pPtr += iStride[1];
  }
}

uint32_t get_file_size(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Failed to open file.\n");
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    uint32_t size = ftell(fp);
    fclose(fp);
    return size;
}

int32_t extract_264(char* srcfile, uint8_t** bitstream, uint64_t* bs_size)
{
  auto* reader = Reader::Create();
  if (reader->initialize(srcfile) != ErrorCode::OK)
  {
      Reader::Destroy(reader);
      printf("reader initialize failed");
      return -1;
  }
  FileInformation info;
  reader->getFileInformation(info);

  Array<ImageId> itemIds;
  // Find item IDs of master images
  reader->getMasterImages(itemIds);

  DecoderConfiguration paramset{};
  reader->getDecoderParameterSets(itemIds[0], paramset);

  // Allocate a buffer for the maximum expected bitstream size
  uint64_t slice_bs_size = get_file_size(srcfile);
  uint8_t *slice_bs = (uint8_t*)malloc(slice_bs_size);
  
  reader->getItemDataWithDecoderParameters(itemIds[0], slice_bs, slice_bs_size);

  // Get the SPS and PPS from the AVC bitstream
  uint8_t* sps_bs = nullptr;
  uint8_t* pps_bs = nullptr;
  uint64_t sps_size = 0;
  uint64_t pps_size = 0;  

  for (const auto& specInfo : paramset.decoderSpecificInfo) {
      if (specInfo.decSpecInfoType == DecoderSpecInfoType::AVC_SPS) {
          sps_bs = (uint8_t*)malloc(specInfo.decSpecInfoData.size);
          sps_bs = (uint8_t*)specInfo.decSpecInfoData.elements;
          sps_size = specInfo.decSpecInfoData.size;
      }
      else if (specInfo.decSpecInfoType == DecoderSpecInfoType::AVC_PPS) {
          pps_bs = (uint8_t*)malloc(specInfo.decSpecInfoData.size);
          pps_bs = (uint8_t*)specInfo.decSpecInfoData.elements;
          pps_size = specInfo.decSpecInfoData.size;
      }
  }

  // Copy the SPS, PPS, and Slice data into a new buffer
  *bs_size = sps_size + pps_size + slice_bs_size;
  *bitstream = (uint8_t*)malloc(*bs_size);
  memcpy(*bitstream, sps_bs, sps_size);
  memcpy(*bitstream + sps_size, pps_bs, pps_size);
  memcpy(*bitstream + sps_size + pps_size, slice_bs, slice_bs_size);

  Reader::Destroy(reader);

  return 0;
}

void H264DecodeInstance (ISVCDecoder* pDecoder, uint8_t* p264Buf, int32_t iFileSize,
                         SBufferInfo *sDstBufInfo,
                         int32_t iErrorConMethod,
                         bool bLegacyCalling) {
  if (pDecoder == NULL) return;

  int32_t iSliceSize;
  int32_t iSliceIndex = 0;
  uint8_t uiStartCode[4] = {0, 0, 0, 1};

  uint8_t* pData[3] = {NULL};

  int32_t iBufPos = 0;
  int32_t iEndOfStreamFlag = 0;
  pDecoder->SetOption (DECODER_OPTION_ERROR_CON_IDC, &iErrorConMethod);

  int32_t iThreadCount = 1;
  pDecoder->GetOption (DECODER_OPTION_NUM_OF_THREADS, &iThreadCount);

  while (true) {

    if (iBufPos >= iFileSize) {
      iEndOfStreamFlag = true;
      if (iEndOfStreamFlag)
        pDecoder->SetOption (DECODER_OPTION_END_OF_STREAM, (void*)&iEndOfStreamFlag);
      break;
    }   

    int i = 0;
    for (i = 0; i < iFileSize; i++) {
      if ((p264Buf[iBufPos + i] == 0 && p264Buf[iBufPos + i + 1] == 0 && p264Buf[iBufPos + i + 2] == 0 && p264Buf[iBufPos + i + 3] == 1
            && i > 0) || (p264Buf[iBufPos + i] == 0 && p264Buf[iBufPos + i + 1] == 0 && p264Buf[iBufPos + i + 2] == 1 && i > 0)) {
        break;
      }
    }
    iSliceSize = i;
    
    if (iSliceSize < 4) { //too small size, no effective data, ignore
      iBufPos += iSliceSize;
      continue;
    }

    pData[0] = NULL;
    pData[1] = NULL;
    pData[2] = NULL;
    if (!bLegacyCalling) {
      pDecoder->DecodeFrameNoDelay (p264Buf + iBufPos, iSliceSize, pData, sDstBufInfo);
    } else {
      pDecoder->DecodeFrame2 (p264Buf + iBufPos, iSliceSize, pData, sDstBufInfo);
    }
    iBufPos += iSliceSize;
    ++ iSliceIndex;
  }

  pDecoder->FlushFrame (pData, sDstBufInfo);
               
  printf ("-------------------------------------------------------\n");
  printf ("Width:\t%d\nheight:\t%d\n", sDstBufInfo->UsrData.sSystemBuffer.iWidth, sDstBufInfo->UsrData.sSystemBuffer.iHeight );
  printf ("-------------------------------------------------------\n");

  if (p264Buf) {
    delete[] p264Buf;
    p264Buf = NULL;
  }
}

void yuv420_to_rgb(unsigned char* yuv[3], unsigned char* rgb, int width, int height, int stride_y, int stride_uv)
{
    int i, j;
    int y, u, v;
    int r, g, b;
    int y_idx, u_idx, v_idx, uv_idx;
    unsigned char* pY = yuv[0];
    unsigned char* pU = yuv[1];
    unsigned char* pV = yuv[2];
    unsigned char* pRGB = rgb;
    int half_width = width / 2;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            y_idx = i * stride_y + j;
            u_idx = (i / 2) * stride_uv + (j / 2);
            v_idx = u_idx;

            y = (int)pY[y_idx];
            u = (int)pU[u_idx];
            v = (int)pV[v_idx];

            // YUV to RGB conversion
            r = (int)(1.164 * (y - 16) + 1.596 * (v - 128));
            g = (int)(1.164 * (y - 16) - 0.813 * (v - 128) - 0.391 * (u - 128));
            b = (int)(1.164 * (y - 16) + 2.018 * (u - 128));

            // Clamp RGB values to [0, 255]
            r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
            g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
            b = (b < 0) ? 0 : ((b > 255) ? 255 : b);

            // Write RGB values to output buffer
            pRGB[0] = (unsigned char)r;
            pRGB[1] = (unsigned char)g;
            pRGB[2] = (unsigned char)b;
            pRGB += 3;
        }
    }
}


gint32 load_heif(gchar *filename)
{
  guint8* bitstream = nullptr;
  guint64 bs_size = 0;
  if (extract_264(filename, &bitstream, &bs_size) != 0) {
    g_message("extract_264 failed");
      // handle error
  }

  ISVCDecoder* pDecoder = NULL;
  SDecodingParam sDecParam = {0};
  bool bLegacyCalling = false;

  sDecParam.sVideoProperty.size = sizeof (sDecParam.sVideoProperty);
  WelsCreateDecoder (&pDecoder);
  pDecoder->Initialize (&sDecParam);

  SBufferInfo *pDstBufInfo;
  pDstBufInfo = new SBufferInfo();

  H264DecodeInstance (pDecoder, bitstream, bs_size,
                    pDstBufInfo,
                    (int32_t)sDecParam.eEcActiveIdc,
                    bLegacyCalling);
  int32_t width = pDstBufInfo->UsrData.sSystemBuffer.iWidth;
  int32_t height = pDstBufInfo->UsrData.sSystemBuffer.iHeight;

  // --- create GIMP image and copy HEIF image into the GIMP image (converting it to RGB)

  gint32 image_ID = gimp_image_new(width, height, GIMP_RGB);
  gimp_image_set_filename(image_ID, filename);

  gint32 layer_ID = gimp_layer_new(image_ID,
                                   _("image content"),
                                   width,height,
                                   GIMP_RGB_IMAGE,
                                   100.0,
                                   GIMP_NORMAL_MODE);

  gboolean success = gimp_image_insert_layer(image_ID,
                                             layer_ID,
                                             0, // gint32 parent_ID,
                                             0); // gint position);
  if (!success) {
    gimp_image_delete(image_ID);
    return LOAD_HEIF_ERROR;
  }

  GimpDrawable *drawable = gimp_drawable_get(layer_ID);
  GimpPixelRgn rgn_out;
  guchar *pRGB = new guchar[width*height*3];

  yuv420_to_rgb(pDstBufInfo->pDst, pRGB, width, height, pDstBufInfo->UsrData.sSystemBuffer.iStride[0], pDstBufInfo->UsrData.sSystemBuffer.iStride[1]);
  gimp_pixel_rgn_init (&rgn_out,
                       drawable,
                       0,0,
                       width,height,
                       TRUE, TRUE);

  gimp_pixel_rgn_set_rect(&rgn_out, pRGB, 0, 0, width, height);
  gimp_drawable_flush (drawable);
  gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
  gimp_drawable_update (drawable->drawable_id,
                        0,0, width,height);
  gimp_drawable_detach(drawable);

  if(pRGB) delete pRGB;
  if(pDstBufInfo) delete pDstBufInfo;

  return image_ID;
}

static void
run (const gchar      *name,
     gint              n_params,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam   values[2];
  GimpPDBStatusType  status = GIMP_PDB_SUCCESS;

  *return_vals  = values;
  *nreturn_vals = 1; // by default only return success code (first parameter)

  textdomain (GETTEXT_PACKAGE);

  gimp_ui_init (PLUGIN_NAME, TRUE);

  GimpRunMode run_mode = static_cast<GimpRunMode>(param[0].data.d_int32);

  if (strcmp (name, LOAD_PROC) == 0) {

    // Make sure all the arguments are there
    if (n_params != 3)
      status = GIMP_PDB_CALLING_ERROR;

    char* filename = param[1].data.d_string;
    int is_interactive = (run_mode == GIMP_RUN_INTERACTIVE);

    if (status == GIMP_PDB_SUCCESS) {
      gint32 gimp_image_ID = load_heif(filename);

      if (gimp_image_ID >= 0) {
        *nreturn_vals = 2;
        values[1].type         = GIMP_PDB_IMAGE;
        values[1].data.d_image = gimp_image_ID;
      }
      else if (gimp_image_ID == LOAD_HEIF_CANCEL) {
        // No image was selected.
        status = GIMP_PDB_CANCEL;
      }
      else {
        status = GIMP_PDB_EXECUTION_ERROR;
      }
    }
  }  
  else {
    status = GIMP_PDB_CALLING_ERROR;
  }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;
}
