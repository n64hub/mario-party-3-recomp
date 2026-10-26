// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs and play_page.mjs: finds patches/survey.c's
// struct in RDRAM by its magic words and offers overlay(), the overlay playing (omovl, the one omOvlGotoEx last went
// to: 0x00-0x46 the minigames, 0x48-0x4E the boards, 0x53 a board's opening, 0x77 the mode select, 0x78 a party's
// setup; the decomp's include/ovl_table.h numbers them all); untilOverlay(overlay, ms), which resolves true once that
// overlay plays, false after ms; go(from, overlay, event, stat, ms), which asks the game to go to overlay (with
// omOvlGotoEx's event and stat) the next time it leaves for `from` (-1: for anywhere) and resolves true once it has gone
// there, false after ms; frames(), the scheduler's frame count (one a frame the game makes: its own rate); mark() and
// rate(), how many frames a second the game made from the one to the other; human(player), that player (0-3) played
// from the keyboard and pad 1 rather than by the computer (bit 0 of the player's stat, a byte at 4 in GwPlayer,
// 0x800D1108, 0x38 bytes a player), which a board's start sets back from the setup.
(() => {
  const magic = (a) => n64r(a) === 0x4d505254 && n64r(a + 4) === 0x53525659;
  if (!window.__survey) {
    // The patches' memory first (patches/patches.ld), then the rest of RDRAM.
    for (let a = 0x80801000; a < 0x81000000 && !window.__survey; a += 4) if (magic(a)) window.__survey = a;
    for (let a = 0x80000000; a < 0x80801000 && !window.__survey; a += 4) if (magic(a)) window.__survey = a;
  }
  const s = window.__survey;
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  window.overlay = () => n64r(s + 32) | 0;
  window.frames = () => n64r(s + 36) >>> 0;
  window.untilOverlay = async (overlay, ms) => {
    const end = Date.now() + ms;
    while (window.overlay() !== overlay && Date.now() < end) await sleep(100);
    return window.overlay() === overlay;
  };
  window.go = async (from, overlay, event, stat, ms) => {
    const warps = n64r(s + 28);
    n64w(s + 12, from >>> 0);
    n64w(s + 16, overlay);
    n64w(s + 20, event);
    n64w(s + 24, stat);
    n64w(s + 8, 1);
    const end = Date.now() + ms;
    while ((n64r(s + 28) === warps || window.overlay() !== overlay) && Date.now() < end) await sleep(100);
    return n64r(s + 28) !== warps && window.overlay() === overlay;
  };
  window.mark = () => { window.__mark = [window.frames(), performance.now()]; return 'frames ' + window.__mark[0]; };
  window.rate = () => {
    const [frames, ms] = [window.frames() - window.__mark[0], performance.now() - window.__mark[1]];
    return `${frames} game frames in ${(ms / 1000).toFixed(1)} s: ${(frames * 1000 / ms).toFixed(1)} a second`;
  };
  window.human = (player) => {
    // The stat byte is the top byte of the word at 4.
    const a = 0x800d110c + player * 0x38;
    n64w(a, (n64r(a) & ~0x01000000) >>> 0);
    return `player ${player} ${n64r(a) & 0x01000000 ? 'computer' : 'human'}`;
  };
  return s ? 'survey at ' + s.toString(16) : 'no survey struct: a page built without patches/survey.c';
})()
