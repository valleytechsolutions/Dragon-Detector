import {crc32} from './badge-codec.js?v=2026-10-23-watcher';

// End each write with a short USB packet, avoiding intermittent native
// USB-JTAG stalls at exact 64-byte boundaries. Also fits legacy 256-byte queues.
export const BADGE_CHUNK_SIZE = 127;

export async function uploadBadge(link, bytes, onProgress = () => {}) {
  link.lines = [];
  await link.write(`badge upload ${bytes.length} ${crc32(bytes).toString(16)}`);
  let reply = await link.wait(s => s === 'BADGE_READY' || s.startsWith('BADGE_ERROR'));
  if (reply !== 'BADGE_READY') throw new Error(reply);
  for (let at = 0; at < bytes.length; at += BADGE_CHUNK_SIZE) {
    const end = Math.min(at + BADGE_CHUNK_SIZE, bytes.length);
    await link.write(bytes.subarray(at, end));
    reply = await link.wait(s => s.startsWith('BADGE_ERROR') || s === `BADGE_ACK ${end}`);
    if (reply.startsWith('BADGE_ERROR')) throw new Error(reply);
    onProgress(Math.round(end * 100 / bytes.length));
  }
  reply = await link.wait(s => s === 'BADGE_OK' || s.startsWith('BADGE_ERROR'));
  if (reply !== 'BADGE_OK') throw new Error(reply);
}
