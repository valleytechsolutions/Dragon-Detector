// One logo-only startup on every board, independent of the saved theme.
// A white field matches the supplied artwork; no rectangular plate is visible.
extern lv_img_dsc_t boot_vts_logo;
static void showStartupLogo(lv_obj_t* parent){
  lv_obj_set_style_bg_color(parent,lv_color_hex(0xffffff),0);
  lv_obj_set_style_bg_opa(parent,LV_OPA_COVER,0);
  auto* img=lv_img_create(parent);lv_img_set_src(img,&boot_vts_logo);lv_obj_center(img);
  lv_obj_clear_flag(img,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_opa(img,LV_OPA_TRANSP,0);
  lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,img);
  lv_anim_set_values(&a,LV_OPA_TRANSP,LV_OPA_COVER);lv_anim_set_time(&a,900);
  lv_anim_set_exec_cb(&a,[](void* o,int32_t v){lv_obj_set_style_opa((lv_obj_t*)o,v,0);});lv_anim_start(&a);
}
