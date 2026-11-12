// gsluminance.cpp: the GS side of the sight-jack's palette view (SightJack::drawPaletteView,
// game/stageparty.cpp). Its own object between menuscene and prgmccard; nothing else calls it.
//
// On PC the file has no code: the host pass behind YamiPan_DrawUiFramePalette
// (frame_palette.frag.hlsl) works the brightness out with these weights and this rounding, each
// channel times its weight over 128, truncated on its own, then summed (white is 251, not 255),
// so the same pixel picks the same ramp entry. No GS memory, no packets.

// FS2 0x002b6b90 gsLuminance(packet, out, work, src, w, h)
// The brightness of the CT32 buffer `src` (w x h; pages are frame pages) as an 8-bit image in
// `out`, summed in the buffer `work`, with scratch pages right after `work` (work + w/64 * h/32),
// in six packet steps:
// FS2 0x002b71b0 gsLumSetContext: context 2 over w x h: no Z write, Z test always, no offset,
//   point sampling, colour clamp on.
// FS2 0x002b6cc0 gsLumWeighGreen: target = (0, G * 0x4b / 128, 0, 0), the source as a modulated
//   texture, one sprite per 64-pixel column.
// FS2 0x002b7b00 gsLumShiftPages: each CT32 page of the source copied into scratch, local to
//   local, through PSMT4 views 8 pixels apart, so its R reads as G and its B as alpha.
// FS2 0x002b75f0 gsLumAddGreen: target G += scratch G * 0x26 / 128 (FBMSK keeps all but G,
//   ALPHA_2 Cs * FIX + Cd).
// FS2 0x002b72b0 gsLumWeighAlpha: scratch G = 0xe * scratch alpha / 128 (untextured sprites,
//   ALPHA_2 Cs * Ad).
// FS2 0x002b75f0 gsLumAddGreen again, at 0x80: target G += scratch G.
// FS2 0x002b7f90 gsLumGreenToHigh: the summed G moved to each word's top byte through PSMCT16
//   views of both buffers (2h rows, sprites 8 pixels over, decal, TEXA AEM), so the result is
//   read as a PSMT8H image over the ramp's CLUT; the scissor goes back to w x h.
// PC: inside the host's pass.
