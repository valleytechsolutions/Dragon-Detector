export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}
export function encodeBadge(rgba, size) {
  if (![360, 466].includes(size) || rgba.length !== size * size * 4) throw new Error('Invalid display size');
  const bytes = new Uint8Array(8 + size * size * 2);
  bytes.set([68, 68, 66, 49, size & 255, size >> 8, size & 255, size >> 8]);
  for (let i = 0; i < size * size; i++) {
    const rgb = ((rgba[i*4] >> 3) << 11) | ((rgba[i*4+1] >> 2) << 5) | (rgba[i*4+2] >> 3);
    bytes[8+i*2] = rgb >> 8; bytes[9+i*2] = rgb & 255;
  }
  return bytes;
}
export function cropGeometry(width, height, size, zoom, x, y) {
  const scale = Math.max(size / width, size / height) * zoom;
  return {x:(size-width*scale)/2+x, y:(size-height*scale)/2+y, width:width*scale, height:height*scale};
}
export function fitCircleZoom(width, height, size) {
  // Inscribe the entire rectangular image in the round screen, including corners.
  return (size / Math.hypot(width, height)) / Math.max(size / width, size / height);
}
