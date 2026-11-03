import {firmwareAtLeast,knownFirmware} from './firmware-version.js?v=0.14.2-guition-beta1';
const full=new Set(['guition_185_beta','waveshare_185b','watcher','lilygo_175','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round','elecrow_21']);
export const hasSessionTools=(board,version)=>full.has(board)&&firmwareAtLeast(version,board==='guition_185_beta'?'0.14.2':board==='waveshare_185b'?'0.14.1':'0.13.0');
export const hasFullLab=(board,version)=>hasSessionTools(board,version)||(board==='waveshare'&&/^0\.(?:9\.(?:[8-9]|\d{2,})|(?:10|11|12)\.\d+)$/.test(version));
export const hasFullRecon=(board,version)=>hasSessionTools(board,version)||(board==='waveshare'&&/^0\.(?:10|11|12)\.\d+$/.test(version));
