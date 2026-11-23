import {boardProfiles} from '../web/board-profiles.js';

// Boards whose firmware this repository builds. The Waveshare P4 Knob 0.14.8
// package is carried over unchanged from the live web flasher
// (tools/fetch_p4_knob_carryover.py) and is checked by p4-knob-carryover.test.js.
export const carriedBoards=new Set(['waveshare_p4_knob']);
export const builtProfiles=Object.fromEntries(Object.entries(boardProfiles).filter(([board])=>!carriedBoards.has(board)));
