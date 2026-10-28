// Evaluated in the page by N64Recomp-browser's tools/record_shaders.mjs and play_page.mjs: finds patches/survey.c's
// struct in RDRAM by its magic words and offers overlay(), the overlay playing (omovl, the one omOvlGotoEx last went
// to: 0x01-0x46 the minigames, 0x48-0x4E the boards, 0x53 a board's opening, 0x70 a minigame's instructions, 0x77
// the mode select, 0x78 a party's setup; the decomp's include/ovl_table.h numbers them all); untilOverlay(overlay, ms),
// which resolves true once that overlay plays, false after ms; go(from, overlay, event, stat, ms, groups, computers),
// which asks the game to go to overlay (with omOvlGotoEx's event and stat: a minigame takes 0 and 0x14, as its
// instructions start it) the next time it leaves for `from` (-1: for anywhere), with the players' teams (groups, four
// of GwPlayer's group, player 1's first: [0, 1, 1, 1] for a 1-vs-3 minigame, [0, 0, 1, 1] for a 2-vs-2; left out,
// they stay) and those the computer plays (computers, four booleans; left out, they stay) set as it goes
// (patches/survey.c), and resolves true once it has gone there, false after ms; frames(), the scheduler's frame count
// (one a frame the game makes: its own rate); mark() and rate(), how many frames a second the game made from the one
// to the other; human(player), that player (0-3) played from the keyboard and pad 1 rather than by the computer (bit 0
// of the player's stat, a byte at 4 in GwPlayer, 0x800D1108, 0x38 bytes a player), which a board's start sets back
// from the setup. A minigame is left as it ends (four computer players end every one), not by a call of omOvlGotoEx
// from elsewhere: its own ending closes its data directory and stops its sound, and without that the next one's files
// don't load, and the processes it leaves running take the heap the next one is given. A minigame is given the teams
// its kind has, as tools/shader_plan.json gives each: with another kind's it reads past its tables and traps (Pump,
// Pump and Away, a 2 vs 2, given 1 vs 3's [0, 1, 1, 1]: "memory access out of bounds" as it starts).
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
  window.go = async (from, overlay, event, stat, ms, groups, computers) => {
    const warps = n64r(s + 28);
    n64w(s + 12, from >>> 0);
    n64w(s + 16, overlay);
    n64w(s + 20, event);
    n64w(s + 24, stat);
    n64w(s + 40, groups ? ((groups[0] << 24) | (groups[1] << 16) | (groups[2] << 8) | groups[3]) >>> 0 : 0xffffffff);
    const bits = computers ? computers.reduce((all, on, player) => all | (on ? 1 << player : 0), 0x80000000) : 0;
    n64w(s + 44, bits >>> 0);
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
