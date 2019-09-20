#include "unown_report.h"

static void VBlankCB(void) {
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void InitUnownReportBg(void) {
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sUnownReportBgTemplates, 3);
    SetBgTilemapBuffer(2, sTilemapBuffer);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    SetGpuReg(REG_OFFSET_BLDCNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BLDALPHA, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BLDY, DISPCNT_MODE_0);
}

static void PrintInstructionsBar(void) {
    u8 color[3] = {0, 2, 3};

    FillWindowPixelBuffer(1, PIXEL_FILL(0));
    AddTextPrinterParameterized3(1, 0, 2, 1, color, 0, (u8 *)gText_Instructions);
    PutWindowTilemap(1);
    CopyWindowToVram(1, 3);
}

static void InitUnownReportWindow(void) {
    InitWindows(sUnownReportWinTemplates);
    DeactivateAllTextPrinters();
    LoadPalette(unownPal, 0xC0, 0x20);
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    PutWindowTilemap(0);
}

void CB2_ShowUnownReport(void) {
    SetVBlankCallback(NULL);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG3CNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG2CNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG1CNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG0CNT, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG3HOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG3VOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG2HOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG2VOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG1HOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG1VOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG0HOFS, DISPCNT_MODE_0);
    SetGpuReg(REG_OFFSET_BG0VOFS, DISPCNT_MODE_0);
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();
    LoadPalette(unownPal, 0, 64);
    sTilemapBuffer = Alloc(0x1000);
    InitUnownReportBg();
    InitUnownReportWindow();
    reset_temp_tile_data_buffers();
    decompress_and_copy_tile_data_to_vram(2, &unownTiles, 0, 0, 0);
    while (free_temp_tile_data_buffers_if_possible());
    LZDecompressWram(unownMap, sTilemapBuffer);
    CopyBgTilemapBufferToVram(2);
    PrintInstructionsBar();
    PrintFirstPage();
    BlendPalettes(-1, 16, 0);
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
    EnableInterrupts(1);
    SetVBlankCallback(VBlankCB);
    SetMainCallback2(MainCB2);
    CreateTask(Task_UnownReportFadeIn, 0);
}

static void MainCB2(void) {
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void Task_UnownReportFadeIn(u8 taskId) {
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_UnownReportWaitForKeyPress;
}

u32 GetCaughtUnown() {
    u32 CaughtUnown;
//  CaughtUnown = VarGet(VAR_UNOWNCAUGHT_PT1) << 16 | VarGet(VAR_UNOWNCAUGHT_PT2);
    CaughtUnown = 0b00001111111111111111111111111111;
    return CaughtUnown;
}

u8 UnownCount() {
    u32 CaughtUnown = GetCaughtUnown();

    u8 UniqueForms = 0;
    for (u8 i = 0; i < 28; i++)
        if (CaughtUnown & (1 << i)) UniqueForms++;
    return UniqueForms;   
}

void PrepareUnownData(u8 taskId);

static u8 GetPage(u8 taskId, u8 SwapDirection) {
    u8 PageNumber;
    struct Task *task = &gTasks[taskId];
    PageNumber = task->data[0];
    u8 count;
    count = UnownCount();
    if (SwapDirection == PAGE_NEXT) {
        if (PageNumber == MAX_PAGE_COUNT) return PageNumber;
        if (PageNumber == 0) {
            if (count == 0) return 5;
            if (count > 1) return 1;
        }
        if (PageNumber == 1) {
            if (count <= UNOWN_PER_PAGE) return 5;
            if (count > UNOWN_PER_PAGE) return 2;
        }
        if (PageNumber == 2) {
            if (count <= 2 * UNOWN_PER_PAGE) return 5;
            if (count > 2 * UNOWN_PER_PAGE) return 3;
        }
        if (PageNumber == 3) {
            if (count > 26) return 4;
            else return 5;
        }
        if (PageNumber >= 5 && FlagGet(ReportFlags[PageNumber-5]) == FALSE) return PageNumber;
        return PageNumber + 1;
    }
    if (SwapDirection == PAGE_PREV) {
        if (PageNumber == 0) return PageNumber;
        if (PageNumber == 5) {
            if (count == 0) return 0;
            if (count <= UNOWN_PER_PAGE) return 1;
            if (count <= 2 * UNOWN_PER_PAGE) return 2;
            if (count <= 26) return 3;
            if (count > 26) return 4;
        }
        return PageNumber - 1;
    }
    return PageNumber;
}

static s8 GetPageNumber(u8 taskId, u8 SwapDirection) {
    struct Task *task = &gTasks[taskId];
    if (GetPage(taskId, SwapDirection) != task->currentPage) {
        task->currentPage = GetPage(taskId, SwapDirection);            
        FillWindowPixelBuffer(0, 0);
        ClearWindowTilemap(0);
        CopyWindowToVram(0, 3);
        return task->currentPage;
    }
    return -1;
}

static void PrintFirstPage() {
    u8 count = UnownCount();
    u16 width = 0;
    StringExpandPlaceholders(gStringVar4, gText_PlayersUnownReport);
    width = GetStringCenterAlignXOffset(2, gStringVar4, 0xFFFF);
    PrintUnownReportText(gStringVar4, 104 - (width >> 1), 48);

    ConvertIntToDecimalStringN(gStringVar1, count, STR_CONV_MODE_LEFT_ALIGN, 4);
    StringExpandPlaceholders(gStringVar4, gText_CurrentKinds);
    width = GetStringCenterAlignXOffset(2, gStringVar4, 0xFFFF);
    PrintUnownReportText(gStringVar4, 104 - (width >> 1), 72);
    PutWindowTilemap(0);
    CopyWindowToVram(0, 3);
}

static u32 UnownFormToPID(u8 form) {
    // In Generation III, Unown's form is determined by the composite value of
    // the least significant 2 bits of each byte in its PID, so we need to
    // "reverse engineer" this PID for the wanted Unown icon
    return ((form & 0b00110000) << 12) | ((form & 0b00001100) << 6) | (form & 0b00000011);
}

static void DisplayUnownIcon(u8 form, u16 x, u16 y) {
    u8 spriteId;
    LoadMonIconPalettes();
    spriteId = CreateMonIcon(PKMN_UNOWN, SpriteCallbackDummy, x, y, 0, UnownFormToPID(form), TRUE);
    gSprites[spriteId].oam.priority = 0;
}

static void PrintUnown(u8 form, u8 row, u8 col) {
    DisplayUnownIcon(form, 48 + col * 88, 52 + (row - col) * 12);
    PrintUnownReportText((u8 *)UnownStrings[form], 48 + col * 88, 48 + (row - col) * 12);
}

static void PrintUnownList(u8 taskId, u8 PageNumber) {
    struct Task *task = &gTasks[taskId];
    
    u32 CaughtUnown = GetCaughtUnown();
    u8 currBit = 0;
    if (PageNumber > 0) currBit = task->data[1 + PageNumber];
    for (u8 row = 0, col = 0; row < UNOWN_PER_PAGE && currBit <= 28; currBit++) {
        if (CaughtUnown & (1 << currBit)) {
            PrintUnown(currBit, row, col);
            row++;
            col ^= 1;
        }   
    }
    if (PageNumber < 3) task->data[2 + PageNumber] = currBit;
    
    PutWindowTilemap(0);
    CopyWindowToVram(0, 3);
}

static void PrintReportPage(u8 PageNumber) {
    PrintUnownReportText((u8 *)ReportStrings[PageNumber], 16, 48);
    PutWindowTilemap(0);
    CopyWindowToVram(0, 3);
}

static void SwapPage(u8 taskId, u8 SwapDirection) {
    s8 PageNumber = GetPageNumber(taskId, SwapDirection);
    if (PageNumber != -1) {
        ResetSpriteData();
        PlaySE(SE_SELECT);
        switch (PageNumber) {
            default:
            case 0:
                PrintFirstPage();
                break;
            case 1:
            case 2:
            case 3:
            case 4:
                PrintUnownList(taskId, PageNumber-1);
                break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
                PrintReportPage(PageNumber - 5);
                break;
        }
    }
}

static void Task_UnownReportWaitForKeyPress(u8 taskId) {
    if (gMain.newAndRepeatedKeys & (DPAD_RIGHT | DPAD_DOWN) || gMain.newKeys & A_BUTTON) SwapPage(taskId, PAGE_NEXT);
    if (gMain.newAndRepeatedKeys & (DPAD_LEFT | DPAD_UP)) SwapPage(taskId, PAGE_PREV);
    if (gMain.newKeys & (B_BUTTON)) {
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_UnownReportFadeOut;
    }
}

static void Task_UnownReportFadeOut(u8 taskId) {
    if (!gPaletteFade.active) {
        Free(sTilemapBuffer);
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
    }
}

static void PrintUnownReportText(u8 *text, u8 var1, u8 var2) {
    u8 color[3] = {0, 2, 3};

    AddTextPrinterParameterized4(0, 1, var1, var2, 0, 0, color, -1, text);
}

void Special_ShowUnownReport(void) {
    SetMainCallback2(CB2_ShowUnownReport);
    ScriptContext2_Enable();
}