const full=new Set(['watcher','lilygo_175','waveshare','waveshare_175','waveshare_175_gps','stopwatch','spotpear','xiao_round','elecrow_21']);
export const hasSessionTools=(board,version)=>full.has(board)&&/^0\.13\.\d+$/.test(version);
export const hasFullLab=(board,version)=>hasSessionTools(board,version)||(board==='waveshare'&&/^0\.(?:9\.(?:[8-9]|\d{2,})|(?:10|11|12)\.\d+)$/.test(version));
export const hasFullRecon=(board,version)=>hasSessionTools(board,version)||(board==='waveshare'&&/^0\.(?:10|11|12)\.\d+$/.test(version));
