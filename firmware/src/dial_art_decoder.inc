// Registered after lv_init. One indexed image is expanded at a time; LVGL asks
// for RGB565+alpha scanlines, avoiding a second full-color image allocation.
static const PackedThemeLogo* dialArt(const void* source){
  for(const auto& item:packedThemeLogos)if(source==item.image)return &item;
  return nullptr;
}
static lv_res_t dialArtInfo(lv_img_decoder_t*,const void* source,lv_img_header_t* out){
  auto* item=dialArt(source);if(!item)return LV_RES_INV;
  *out=item->image->header;out->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;
}
static lv_res_t dialArtOpen(lv_img_decoder_t*,lv_img_decoder_dsc_t* dsc){
  auto* item=dialArt(dsc->src);if(!item)return LV_RES_INV;
  // A malformed/generated oversized image must never exhaust the radio heap.
  if(item->image->data_size>64*1024)return LV_RES_INV;
  auto* bytes=(uint8_t*)malloc(item->image->data_size);
  if(!bytes)return LV_RES_INV;
  if(!dragon::unpackBlock(item->bytes,item->length,bytes,item->image->data_size)){free(bytes);return LV_RES_INV;}
  dsc->user_data=bytes;dsc->img_data=nullptr;return LV_RES_OK;
}
static lv_res_t dialArtLine(lv_img_decoder_t*,lv_img_decoder_dsc_t* dsc,lv_coord_t x,lv_coord_t y,lv_coord_t length,uint8_t* out){
  const auto* item=dialArt(dsc->src);if(!item||!dsc->user_data||x<0||y<0||length<0||x+length>item->image->header.w||y>=item->image->header.h)return LV_RES_INV;
  const auto cf=item->format;
  unsigned bits=cf==LV_IMG_CF_INDEXED_1BIT?1:cf==LV_IMG_CF_INDEXED_2BIT?2:cf==LV_IMG_CF_INDEXED_4BIT?4:cf==LV_IMG_CF_INDEXED_8BIT?8:0;
  if(!bits)return LV_RES_INV;
  const auto* bytes=(const uint8_t*)dsc->user_data;
  const unsigned palette=(1u<<bits)*4,stride=(item->image->header.w*bits+7)/8;
  if(palette+(y+1)*stride>item->image->data_size)return LV_RES_INV;
  for(int i=0;i<length;++i){unsigned bit=(x+i)*bits,index=(bytes[palette+y*stride+bit/8]>>(8-bits-bit%8))&((1u<<bits)-1);
    const uint8_t* rgba=bytes+index*4;lv_color_t pixel=lv_color_make(rgba[2],rgba[1],rgba[0]);
    memcpy(out+i*3,&pixel,2);out[i*3+2]=rgba[3];
  }
  return LV_RES_OK;
}
static void dialArtClose(lv_img_decoder_t*,lv_img_decoder_dsc_t* dsc){free(dsc->user_data);dsc->user_data=nullptr;}
bool prepareThemeLogos(){
  static bool initialized=false;if(initialized)return true;
  auto* decoder=lv_img_decoder_create();if(!decoder)return false;
  lv_img_cache_set_size(1);
  lv_img_decoder_set_info_cb(decoder,dialArtInfo);lv_img_decoder_set_open_cb(decoder,dialArtOpen);
  lv_img_decoder_set_read_line_cb(decoder,dialArtLine);lv_img_decoder_set_close_cb(decoder,dialArtClose);
  // LVGL rejects variable images with null data before consulting decoders.
  // RAW_ALPHA also prevents its built-in decoder from interpreting compressed
  // bytes as indexed pixels if our bounded allocation ever fails.
  for(const auto& item:packedThemeLogos){item.image->data=item.bytes;item.image->header.cf=LV_IMG_CF_RAW_ALPHA;}
  initialized=true;return true;
}
