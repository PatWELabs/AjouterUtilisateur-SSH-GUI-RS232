/*
 * AjoutUtilisateurSSH_RS232_GUI.cpp
 * Win32 GUI — Style Windows 2000 Pro
 * Toolbar 3D + Icônes 16x16 (TB1)
 * Onglets T3 (5 tabs)
 * Console custom C2 (DrawText + buffer circulaire)
 * Fenêtre fixe 
 */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AjoutUtilisateurSSH_RS232_GUI.h"
#include "resource.h"
#define IDC_COMPORTS     600
#define IDC_BAUDRATES    601
#define IDC_BTN_CONNECT  602
#define CMD_VENDOR	   302  
#pragma comment(lib, "comctl32.lib")
#pragma warning(disable:4996) // Desactive les warnings sur les fonctions "insecure" comme sprintf
#pragma warning(disable:4100) // Desactive les warnings sur les paramètres non utilises (ex: DialogProc)
#pragma warning(disable:4244) // Desactive les warnings sur les conversions implicites (ex: int to DWORD)
#pragma warning(disable:28251) // Desactive les warnings sur les fonctions de securite (ex: CreateFileA vs CreateFileW)
#pragma warning(disable:6262) // Desactive les warnings sur les buffers de grande taille (ex: char buf[512])
#pragma warning(disable:6001) // Desactive les warnings sur l'utilisation de variables non initialisees (ex: DCB dcb; sans memset)
#pragma warning(disable:6031) // Desactive les warnings sur les fonctions dont on ignore le retour (ex: Sleep(1000); sans verifier le resultat)
#pragma warning(disable:26451) // Desactive les warnings sur les depassements de buffer potentiels (ex: strcpy sans verifier la taille)
#pragma warning(disable:6054) // Desactive les warnings sur les chaînes de caractères non null-terminees (ex: char buf[256]; sans initialiser à 0)  
constexpr auto APP_TITLE = L"Provision SSH GUI — Win64  Patrice Waechter-Ebling";

#define WIN_WIDTH   900
#define WIN_HEIGHT  650
#define IDC_COMPORTS     600
#define IDC_BAUDRATES    601
#define CONSOLE_LINES 2000
#define CONSOLE_COLS  512
#define COL_DEFAULT RGB(220,220,220)
#define COL_INFO    RGB(180,220,255)
#define COL_ALERTE  RGB(255,220,150)
#define COL_ERREUR  RGB(255,150,150)
#define COL_SUCCES  RGB(255,150,150)
#define COL_RX      RGB(180,255,180)
#define COL_TX      RGB(0,255,0)

WNDCLASS wc = { 0 };
HWND g_hMainWnd;
HWND g_hToolbar;
HWND g_hConsole;
HWND g_hComboPorts;
HWND g_hComboBaud;
HWND g_hBtnConnect;
HFONT g_hFontConsole;

HANDLE g_hSerial = INVALID_HANDLE_VALUE;
volatile int gRunRX = 0;
char gConsoleBuf[CONSOLE_LINES][CONSOLE_COLS];
int gConsoleHead = 0;
int gConsoleCount = 0;
char gVendor[64];
int  gPrompt = 0;

LRESULT CALLBACK ProcedurePrincipale(HWND, UINT, WPARAM, LPARAM);
void CreerBarreOutil(HWND);
void InitToolbarButtons();
void InitConsole(HWND);
void LayoutConnexionArea(HWND);
void ConsoleAddColored(const char* text, COLORREF color);
void ConsolePaint(HWND, HDC);
void DetectCOMPorts(HWND hCombo);
int OpenCOM(const char* port);
void CloseCOM();
DWORD WINAPI RXThread(LPVOID lp);
int AutoBaud(DWORD* foundBaud);
void DetectVendor();
void BuildSSHScript(char script[][256], int* count, const char* user, const char* pass);
void SendSSHScript(char script[][256], int count, const char* enablePassword);

void SetFixedWindow(HWND hWnd)
{
    LONG style = GetWindowLong(hWnd, GWL_STYLE);
    style &= ~WS_MAXIMIZEBOX;
    style &= ~WS_THICKFRAME;
    SetWindowLong(hWnd, GWL_STYLE, style);
}
void ConsoleAddColored(const char* text, COLORREF color)
{
    int idx = gConsoleHead;

    strncpy(gConsoleBuf[idx], text, CONSOLE_COLS - 1);
    gConsoleBuf[idx][CONSOLE_COLS - 1] = 0;

    gConsoleHead = (gConsoleHead + 1) % CONSOLE_LINES;
    if (gConsoleCount < CONSOLE_LINES)
        gConsoleCount++;

    InvalidateRect(g_hConsole, NULL, TRUE);
}
void ConsolePaint(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    SelectObject(hdc, g_hFontConsole);
    SetBkMode(hdc, TRANSPARENT);

    int lineHeight = 16;
    int y = rc.bottom - lineHeight;

    int idx = gConsoleHead - 1;
    if (idx < 0) idx = CONSOLE_LINES - 1;

    int count = gConsoleCount;

    while (count > 0 && y >= 0)
    {
        const char* s = gConsoleBuf[idx];

        COLORREF col = COL_DEFAULT;

        if (strstr(s, "[RX]") == s) col = COL_RX;
        else if (strstr(s, "[TX]") == s) col = COL_TX;
        else if (strstr(s, "[INFO]") == s) col = COL_INFO;
        else if (strstr(s, "[WARN]") == s) col = COL_ALERTE;
        else if (strstr(s, "[ERR]") == s) col = COL_ERREUR;

        SetTextColor(hdc, col);
        TextOutA(hdc, 5, y, s, strlen(s));

        y -= lineHeight;

        idx--;
        if (idx < 0) idx = CONSOLE_LINES - 1;
        count--;
    }
}
LRESULT CALLBACK ConsoleProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        ConsolePaint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}
void RegisterConsoleClass()
{
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ConsoleProc;
    wc.hInstance = wc.hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ConsoleCustomClass";

    RegisterClass(&wc);
}
void InitConsole(HWND hParent)
{
    RegisterConsoleClass();

    g_hConsole = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"ConsoleCustomClass",
        L"",
        WS_CHILD | WS_VISIBLE,
        10, 150, 880, 480,
        hParent,
        NULL,
        wc.hInstance,
        NULL);

    g_hFontConsole = CreateFont(
        14, 0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH,
        L"Consolas");

    ConsoleAddColored("[INFO] Console initialisee.", COL_INFO);
}
void SerialSendRaw(const char* s)
{
    if (g_hSerial == INVALID_HANDLE_VALUE)
    {
        ConsoleAddColored("[ERR] Port serie non ouvert.", COL_ERREUR);
        return;
    }

    DWORD written = 0;
    WriteFile(g_hSerial, s, strlen(s), &written, NULL);

    char buf[CONSOLE_COLS];
    sprintf(buf, "[TX] %s", s);
    ConsoleAddColored(buf, COL_TX);
}
void SerialSendLine(const char* s)
{
    char out[CONSOLE_COLS];
    sprintf(out, "%s\r", s);
    SerialSendRaw(out);
}
int SerialReadNonBlocking(char* buf, int maxLen, DWORD timeoutMs)
{
    DWORD start = GetTickCount();
    DWORD total = 0;

    while ((GetTickCount() - start) < timeoutMs && total < (DWORD)(maxLen - 1))
    {
        DWORD read = 0;
        if (ReadFile(g_hSerial, buf + total, maxLen - 1 - total, &read, NULL) && read > 0)
        {
            total += read;
        }
        else
        {
            Sleep(10);
        }
    }

    buf[total] = 0;
    return (int)total;
}
DWORD WINAPI RXThread(LPVOID lp)
{
    char buf[CONSOLE_COLS];
    DWORD read;

    ConsoleAddColored("[INFO] Thread RX demarre.", COL_INFO);

    while (gRunRX)
    {
        if (ReadFile(g_hSerial, buf, CONSOLE_COLS - 1, &read, NULL) && read > 0)
        {
            buf[read] = 0;

            /* Nettoyage CR/LF */
            for (DWORD i = 0; i < read; i++)
            {
                if (buf[i] == '\r' || buf[i] == '\n')
                    buf[i] = ' ';
            }

            char line[CONSOLE_COLS];
            sprintf(line, "[RX] %s", buf);
            ConsoleAddColored(line, COL_RX);
        }
        else
        {
            Sleep(10);
        }
    }

    ConsoleAddColored("[WARN] Thread RX termine.", COL_ALERTE);
    return 0;
}
int OpenCOM(const char* port)
{
g_hSerial = CreateFileA(port,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);

    if (g_hSerial == INVALID_HANDLE_VALUE)
    {
        ConsoleAddColored("[ERR] Impossible d'ouvrir le port COM.", COL_ERREUR);
        return 0;
    }

    /* Configuration par defaut */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(g_hSerial, &dcb))
    {
        ConsoleAddColored("[ERR] GetCommState a echoue.", COL_ERREUR);
        CloseCOM();
        return 0;
    }

    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(g_hSerial, &dcb))
    {
        ConsoleAddColored("[ERR] SetCommState a echoue.", COL_ERREUR);
        CloseCOM();
        return 0;
    }

    COMMTIMEOUTS t;
    memset(&t, 0, sizeof(t));
    t.ReadIntervalTimeout = 50;
    t.ReadTotalTimeoutConstant = 50;
    t.ReadTotalTimeoutMultiplier = 10;
    t.WriteTotalTimeoutConstant = 50;
    t.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(g_hSerial, &t);

    PurgeComm(g_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);

    ConsoleAddColored("[INFO] Port COM ouvert avec succes.", COL_INFO);
    return 1;
}
void CloseCOM()
{
    if (g_hSerial != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hSerial);
        g_hSerial = INVALID_HANDLE_VALUE;
        ConsoleAddColored("[WARN] Port COM ferme.", COL_ALERTE);
    }
}
int AutoBaud(DWORD* foundBaud)
{
    DWORD bauds[] = { 9600, 19200, 38400, 57600, 115200 };
    char buf[CONSOLE_COLS];

    ConsoleAddColored("[INFO] Auto-baud en cours...", COL_INFO);

    for (int i = 0; i < 5; i++)
    {
        DWORD b = bauds[i];

        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);

        if (!GetCommState(g_hSerial, &dcb))
            continue;

        dcb.BaudRate = b;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;

        if (!SetCommState(g_hSerial, &dcb))
            continue;

        PurgeComm(g_hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);

        SerialSendRaw("\r\r\r");
        Sleep(300);

        int n = SerialReadNonBlocking(buf, CONSOLE_COLS, 800);
        if (n > 0)
        {
            int printable = 0;
            for (int k = 0; k < n; k++)
            {
                if (buf[k] >= 0x20 && buf[k] <= 0x7E)
                {
                    printable = 1;
                    break;
                }
            }

            if (printable)
            {
                *foundBaud = b;

                char msg[128];
                sprintf(msg, "[INFO] Baud detecte : %lu", b);
                ConsoleAddColored(msg, COL_INFO);

                return 1;
            }
        }
    }

    ConsoleAddColored("[ERR] Echec de l'auto-baud.", COL_ERREUR);
    return 0;
}
int DetectPromptInBuffer(const char* s)
{
    if (strstr(s, "(config") != NULL)
        return 3;

    if (strchr(s, '#') != NULL)
        return 2;

    if (strchr(s, '>') != NULL)
        return 1;

    return 0;
}
void DetectVendor()
{
    char buf[CONSOLE_COLS];
    memset(gVendor, 0, sizeof(gVendor));
    strcpy(gVendor, "Unknown");

    ConsoleAddColored("[INFO] Detection du vendor...", COL_INFO);

    SerialSendLine("show version");
    Sleep(1500);

    int n = SerialReadNonBlocking(buf, CONSOLE_COLS, 1000);
    if (n <= 0)
    {
        ConsoleAddColored("[ERR] Aucune reponse au show version.", COL_ERREUR);
        return;
    }

    if (strstr(buf, "Cisco") != NULL)
        strcpy(gVendor, "Cisco");
    else if (strstr(buf, "Aruba") != NULL)
        strcpy(gVendor, "Aruba");
    else if (strstr(buf, "HP ") != NULL || strstr(buf, "ProCurve") != NULL)
        strcpy(gVendor, "HP");
    else if (strstr(buf, "Juniper") != NULL)
        strcpy(gVendor, "Juniper");

    char msg[128];
    sprintf(msg, "[INFO] Vendor detecte : %s", gVendor);
    ConsoleAddColored(msg, COL_INFO);
}
void BuildSSHScript(char script[][256], int* count,const char* user, const char* pass)
{
    int i = 0;

    for (int k = 0; k < 100; k++)
        memset(script[k], 0, 256);

    ConsoleAddColored("[INFO] Generation du script SSH...", COL_INFO);

    if (strcmp(gVendor, "Cisco") == 0)
    {
        sprintf(script[i++], "username %s privilege 15 secret %s", user, pass);
        sprintf(script[i++], "ip ssh version 2");
        sprintf(script[i++], "line vty 0 4");
        sprintf(script[i++], "login local");
        sprintf(script[i++], "transport input ssh");
        sprintf(script[i++], "end");
        sprintf(script[i++], "wr mem");
    }
    else if (strcmp(gVendor, "HP") == 0 || strcmp(gVendor, "Aruba") == 0)
    {
        sprintf(script[i++], "password manager user-name %s plaintext %s", user, pass);
        sprintf(script[i++], "crypto key generate ssh rsa");
        sprintf(script[i++], "ip ssh");
        sprintf(script[i++], "write memory");
    }
    else if (strcmp(gVendor, "Juniper") == 0)
    {
        sprintf(script[i++], "set system login user %s class super-user", user);
        sprintf(script[i++], "set system login user %s authentication plain-text-password", user);
        sprintf(script[i++], "# Tapez le mot de passe manuellement : %s", pass);
        sprintf(script[i++], "commit");
        sprintf(script[i++], "exit");
    }
    else
    {
        sprintf(script[i++], "echo Vendor inconnu, script non disponible");
    }

    *count = i;

    char msg[128];
    sprintf(msg, "[INFO] Script genere (%d lignes).", *count);
    ConsoleAddColored(msg, COL_INFO);
}
void SendSSHScript(char script[][256], int count, const char* enablePassword)
{
    ConsoleAddColored("[INFO] Envoi du script SSH...", COL_INFO);

    for (int i = 0; i < count; i++)
    {
        if (strstr(script[i], "username") ||
            strstr(script[i], "ip ssh") ||
            strstr(script[i], "line vty") ||
            strstr(script[i], "password manager") ||
            strstr(script[i], "crypto key") ||
            strstr(script[i], "set system"))
        {
            SerialSendLine("enable");
            Sleep(300);

            if (enablePassword && enablePassword[0])
            {
                SerialSendLine(enablePassword);
                Sleep(300);
            }

            SerialSendLine("conf t");
            Sleep(300);
        }

        SerialSendLine(script[i]);
        Sleep(300);

        if (strcmp(script[i], "end") == 0)
        {
            SerialSendLine("end");
            Sleep(300);
        }
    }

    ConsoleAddColored("[INFO] Script SSH envoye avec succes.", COL_INFO);
}
void DetectCOMPorts(HWND hCombo)
{
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    CHAR devices[65536];
    DWORD len = QueryDosDeviceA(NULL, devices, sizeof(devices));
    if (len == 0)
    {
        ConsoleAddColored("[ERR] QueryDosDevice a echoue.", COL_ERREUR);return;
    }else {
		ConsoleAddColored("[INFO] Detection des ports COM...", COL_INFO);
    }
    CHAR* ptr = devices;
    while (*ptr)
    {
        if (strncmp(ptr, "COM", 3) == 0){SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)ptr);}
        ptr += strlen(ptr) + 1;
    }
    SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}
void CreerInterfaceConnexionRS232(HWND hWnd)
{
    CreateWindow(L"STATIC",L"Port:",WS_CHILD | WS_VISIBLE,20, 60, 40, 20,hWnd,NULL,wc.hInstance,NULL);
    g_hComboPorts = CreateWindowEx(WS_EX_CLIENTEDGE,WC_COMBOBOX,L"",WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,65, 55, 120, 200,hWnd,(HMENU)IDC_COMPORTS,wc.hInstance,NULL);
    CreateWindow(L"STATIC",L"Baud:",WS_CHILD | WS_VISIBLE,200, 60, 40, 20,hWnd,NULL,wc.hInstance,NULL);
    g_hComboBaud = CreateWindowEx(WS_EX_CLIENTEDGE,WC_COMBOBOX,L"9600",WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,245, 55, 120, 200,hWnd,(HMENU)IDC_BAUDRATES,wc.hInstance,NULL);
    g_hBtnConnect = CreateWindow(WC_BUTTON,L"Connecter",WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,380, 55, 120, 26,hWnd,(HMENU)CMD_CONNECT,wc.hInstance,NULL);
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"9600");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"19200");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"38400");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"57600");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"115200");
    SendMessage(g_hComboBaud, CB_SETCURSEL, 0, 0);
    DetectCOMPorts(g_hComboPorts);
    ConsoleAddColored("[INFO] Zone Connexion initialisee.", COL_SUCCES);
}
void HandleConnectButton()
{
    char port[64];
    char baudStr[32];
    int selPort = SendMessage(g_hComboPorts, CB_GETCURSEL, 0, 0);
    if (selPort == CB_ERR){ConsoleAddColored("[ERR] Selection invalide.", COL_ERREUR);return;}
    SendMessage(g_hComboPorts, CB_GETLBTEXT, selPort, (LPARAM)(LPWSTR)port);
    SendMessage(g_hComboBaud, CB_GETLBTEXT, SendMessage(g_hComboBaud, CB_GETCURSEL, 0, 0) , (LPARAM)(LPWSTR)baudStr);
    ConsoleAddColored(port, COL_INFO);
    DWORD baud = atol(baudStr);
    ConsoleAddColored("[INFO] Ouverture du port selectionne...", COL_INFO);
    ConsoleAddColored("port", COL_TX);
    if (OpenCOM(port))
    {
        DCB dcb;
        GetCommState(g_hSerial, &dcb);
        dcb.BaudRate = baud;
        SetCommState(g_hSerial, &dcb);
        gRunRX = 1;
        CreateThread(NULL, 0, RXThread, NULL, 0, NULL);

        ConsoleAddColored("[INFO] Connecte avec succes.", COL_INFO);
    }else{
        ConsoleAddColored("[ERR] Impossible d'ouvrir le port.", COL_ERREUR);
    }
}
void CreerBarreOutil(HWND hWnd)
{
    TBBUTTON tb[6];
    memset(tb, 0, sizeof(tb));
    g_hToolbar = CreateToolbarEx(hWnd,WS_CHILD | WS_VISIBLE  | CCS_TOP,IDR_MAIN_TOOLBAR,6,wc.hInstance,IDR_MAIN_TOOLBAR,NULL,0,16, 16, 32, 24,sizeof(TBBUTTON));
    HIMAGELIST hImgList = ImageList_Create(24, 24, ILC_COLOR24 | ILC_MASK, 6, 0);
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(CONNECT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(BAUD), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(VENDEUR), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(SCRIPT), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(ENVOYER), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    ImageList_AddMasked(hImgList, (HBITMAP)LoadImage(wc.hInstance, MAKEINTRESOURCE(CONSOLE), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION), RGB(255, 0, 255));
    tb[0].iBitmap = 0;
    tb[0].idCommand = CMD_CONNECT;
    tb[0].fsState = TBSTATE_ENABLED;
    tb[0].fsStyle = TBSTYLE_BUTTON;
	tb[0].iString = (INT_PTR)L"Connecter";
    tb[1].iBitmap = 1;
    tb[1].idCommand = CMD_AUTOBAUD;
    tb[1].fsState = TBSTATE_ENABLED;
    tb[1].fsStyle = TBSTYLE_BUTTON;
    tb[1].iString = (INT_PTR)L"Vitesse";
    tb[2].iBitmap = 2;
    tb[2].idCommand = CMD_VENDOR;
    tb[2].fsState = TBSTATE_ENABLED;
    tb[2].fsStyle = TBSTYLE_BUTTON;
    tb[2].iString = (INT_PTR)L"Vendeur";
    tb[3].iBitmap = 3;
    tb[3].idCommand = CMD_SCRIPT;
    tb[3].fsState = TBSTATE_ENABLED;
    tb[3].fsStyle = TBSTYLE_BUTTON;
    tb[3].iString = (INT_PTR)L"Script";
    tb[4].iBitmap = 4;
    tb[4].idCommand = CMD_SENDSCRIPT;
    tb[4].fsState = TBSTATE_ENABLED;
    tb[4].fsStyle = TBSTYLE_BUTTON;
    tb[4].iString = (INT_PTR)L"Envoyer";
    tb[5].iBitmap = 5;
    tb[5].idCommand = CMD_CONSOLE;
    tb[5].fsState = TBSTATE_ENABLED;
    tb[5].fsStyle = TBSTYLE_BUTTON;
    SendMessage(g_hToolbar, TB_ADDBUTTONS, 6, (LPARAM)&tb);
    SendMessage(g_hToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImgList);
    SendMessage(g_hToolbar, TB_AUTOSIZE, 0, 0);
}

LRESULT CALLBACK ProcedurePrincipale(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hMainWnd = hWnd;
        InitConsole(hWnd);

        SetFixedWindow(hWnd);

        ConsoleAddColored("[INFO] Fenetre principale creee.", COL_INFO);
        return 0;
    }
    case WM_SIZE:
    {
        SendMessage(g_hToolbar, TB_AUTOSIZE, 0, 0);
        MoveWindow(g_hConsole,10,150,LOWORD(lParam) - 20,HIWORD(lParam) - 160,TRUE);

        return 0;
    }

    /******************************************************
     * COMMANDES (toolbar + bouton Connecter)
     ******************************************************/
    case WM_COMMAND:
    {
    case CMD_CONNECT:
        HandleConnectButton();
        break;

    case CMD_AUTOBAUD:
    {
        if (g_hSerial == INVALID_HANDLE_VALUE)
        {
            ConsoleAddColored("[ERR] Port non ouvert.", COL_ERREUR);
            return -1;
        }

        DWORD baud = 0;
        AutoBaud(&baud);
        break;
    }

    case CMD_VENDOR:
    {
        if (g_hSerial == INVALID_HANDLE_VALUE)
        {
            ConsoleAddColored("[ERR] Port non ouvert.", COL_ERREUR);
            return -1;
        }

        DetectVendor();
        break;
    }

    case CMD_SCRIPT:
    {
        if (strcmp(gVendor, "Unknown") == 0)
        {
            ConsoleAddColored("[WARN] Vendor non detecte.", COL_ALERTE);
            return -1;
        }

        char user[64] = "admin";
        char pass[64] = "password";

        char script[100][256];
        int count = 0;

        BuildSSHScript(script, &count, user, pass);

        ConsoleAddColored("[INFO] Apercu du script SSH :", COL_INFO);
        for (int i = 0; i < count; i++)
        {
            char buf[300];
            sprintf(buf, "  %02d) %s", i + 1, script[i]);
            ConsoleAddColored(buf, COL_INFO);
        }

        break;
    }

    case CMD_SENDSCRIPT:
    {
        if (strcmp(gVendor, "Unknown") == 0)
        {
            ConsoleAddColored("[WARN] Vendor non detecte.", COL_ALERTE);
            return -1;
        }

        char user[64] = "admin";
        char pass[64] = "password";

        char script[100][256];
        int count = 0;

        BuildSSHScript(script, &count, user, pass);
        SendSSHScript(script, count, "");

        break;
    }

    case CMD_CONSOLE:
        ConsoleAddColored("[INFO] Console activee.", COL_INFO);
        break;
    case  IDC_BTN_CONNECT:
            HandleConnectButton();
            break;

        return 0;
    }
    case WM_CLOSE:
    {
        if (gRunRX)
        {
            gRunRX = 0;
            Sleep(200);
        }

        CloseCOM();
        DestroyWindow(hWnd);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR     lpCmdLine,int       nCmdShow){
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ProcedurePrincipale;
    wc.hInstance = wc.hInstance;
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"AjoutUtilisateurSSH_RS232_GUI";
    if (!RegisterClass(&wc))
    {
        return MessageBox(NULL,L"Erreur: impossible d'enregistrer la classe fenetre.",L"Erreur",MB_ICONERROR);
    }
    HWND hWnd = CreateWindowEx(0, L"AjoutUtilisateurSSH_RS232_GUI", APP_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, WIN_WIDTH, WIN_HEIGHT, NULL, NULL, wc.hInstance, NULL);
    if (!hWnd)
    {
        return MessageBox(NULL,L"Erreur: impossible de creer la fenetre principale.",L"Erreur",MB_ICONERROR);
    }
    CreerBarreOutil(hWnd);
    CreateWindow(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE, 20, 60, 40, 20, hWnd, NULL, wc.hInstance, NULL);
    g_hComboPorts = CreateWindowEx(WS_EX_CLIENTEDGE, WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 65, 55, 120, 200, hWnd, (HMENU)IDC_COMPORTS, wc.hInstance, NULL);
    CreateWindow(L"STATIC", L"Baud:", WS_CHILD | WS_VISIBLE, 200, 60, 40, 20, hWnd, NULL, wc.hInstance, NULL);
    g_hComboBaud = CreateWindowEx(WS_EX_CLIENTEDGE, WC_COMBOBOX, L"9600", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 245, 55, 120, 200, hWnd, (HMENU)IDC_BAUDRATES, wc.hInstance, NULL);
    g_hBtnConnect = CreateWindow(WC_BUTTON, L"Connecter", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 380, 55, 120, 26, hWnd, (HMENU)CMD_CONNECT, wc.hInstance, NULL);
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"9600");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"19200");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"38400");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"57600");
    SendMessage(g_hComboBaud, CB_ADDSTRING, 0, (LPARAM)L"115200");
    SendMessage(g_hComboBaud, CB_SETCURSEL, 0, 0);
    DetectCOMPorts(g_hComboPorts);
    ConsoleAddColored("[INFO] Zone Connexion initialisee.", COL_SUCCES);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
