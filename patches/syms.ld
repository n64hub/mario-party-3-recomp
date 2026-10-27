/* The natives the patches call: each address is a dummy N64Recomp turns into a call of the C function of that name
   (the runtime's <libultra function>_recomp, or src/game's). patches.h renames the libultra calls. */
__start = 0x80000000;

recomp_load_overlays = 0x8F000000;
osRecvMesg_recomp = 0x8F000004;
osEPiStartDma_recomp = 0x8F000008;
recomp_prc_resume = 0x8F00000C;
recomp_prc_yield = 0x8F000010;
recomp_prc_exit = 0x8F000014;
recomp_prc_reset = 0x8F000018;
recomp_survey_register = 0x8F00001C;
recomp_hud_4x3 = 0x8F000020;
