// sethook.cpp : Defines the entry point for the console application.
//

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <Shlwapi.h>
#include <olectl.h>
#pragma comment( lib, "Shlwapi.lib" )

using namespace std;

namespace ID_ELEMS
{
  const DWORD ID_BTNSTART = WM_USER + 100;
  const DWORD ID_BTNSTOP = ID_BTNSTART + 1;
  const DWORD ID_BTNHOOK = ID_BTNSTOP + 1;
  const DWORD ID_EDITSTART = ID_BTNHOOK + 1;
  const DWORD ID_EDITSTOP = ID_EDITSTART + 1;
  const DWORD ID_LABEL = ID_EDITSTOP + 1;
  const DWORD ID_SHOWHIDE = ID_LABEL + 1;
  const DWORD ID_OPENFULLSCREEN = ID_SHOWHIDE + 1;
  const DWORD ID_LABELSHPORTCUT = ID_OPENFULLSCREEN + 1;
  const DWORD ID_PANELSHORTCUT = ID_LABELSHPORTCUT + 1;
  const DWORD ID_SHOWSHORTCUT = ID_PANELSHORTCUT + 1;
  const DWORD ID_EDITTIMER = ID_SHOWSHORTCUT + 1;
  const DWORD ID_TIMER = ID_EDITTIMER + 1;
}

struct SCommonData
{
  HHOOK hook, hookc;
  DWORD pid, tid;
  DWORD utimer;
  HWND hwndMain;
  HWND hwnd;
  HWND btn_start;
  HWND edit_start;
  HWND btn_stop;
  HWND edit_stop;
  HWND btn_hook;
  HWND label_hook;
  HWND btn_showhide;
  HWND btn_fullscreen;
  HWND btn_timer;
  HWND hwnd_shortcut;
  HWND label_panels;
  HWND edit_timer;
  LPSTR pathToFile;
  LPSTR windowName;
  WORD countWinFromEdit;
  WORD numWinForCloseFromEdit;
};

struct SProcessData
{
  STARTUPINFOA st;
  PROCESS_INFORMATION pi;
  BOOL status;
  BOOL status_thread;
  HWND mainHWND;
};

static SCommonData common;

class TCreateProcess
{
public:
  TCreateProcess() : timeout(0), isFullscreen(false) {}

  BOOL CreateProcess(char* path, char *windowname, WORD createCount = 1)
  {
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
      return false;
    BOOL isCreated = false;
    while (createCount-- > 0)
    {
      DWORD id;
      SProcessData one = {};

      isCreated = CreateProcessA(path, NULL, 0, 0, true, REALTIME_PRIORITY_CLASS, 0, 0, &one.st, &one.pi);
      common.tid = one.pi.dwThreadId;

      HANDLE hp = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&TCreateProcess::WaitForWindowName, windowname, 0, &id);
      BOOL isTimeout = WaitForSingleObject(hp, timeout) == WAIT_TIMEOUT;

      one.status = isCreated;
      one.status_thread = isTimeout;
      one.mainHWND = common.hwnd;
      pData.push_back(one);
    }
    return isCreated;
  }

  BOOL adjustSizesOnFullScreen(DWORD numberWin)
  {
    if (numberWin < pData.size())
    {
      RECT rect;
      SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
      return SetWindowPos(pData.at(numberWin).mainHWND, NULL, 0, 0, rect.right, // GetSystemMetrics(SM_CXMAXIMIZED)
        rect.bottom, NULL);
    }
    return false;
  }

  BOOL adjustAllSizesOnFullScreen()
  {
    BOOL stat = true;
    RECT rect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
    for (vector<SProcessData>::iterator it = pData.begin(); it != pData.end(); ++it)
      stat = stat ? SetWindowPos(it->mainHWND, NULL, 0, 0, rect.right, // GetSystemMetrics(SM_CXMAXIMIZED)
      rect.bottom, NULL) : false;
    return stat;
  }

  BOOL setTimeout(DWORD msec)
  {
    return msec < 0 ? false : (timeout = msec);
  }

  vector<SProcessData> *getProcessData()
  {
    return &pData;
  }

  void showAllWindows(bool show)
  {
    for (vector<SProcessData>::iterator it = pData.begin(); it != pData.end(); ++it)
      ShowWindow(it->mainHWND, show ? SW_SHOW : SW_HIDE);
  }

  void setFullscreen()
  {
    isFullscreen = true;
  }

  void resetFullscreen()
  {
    isFullscreen = false;
  }

  BOOL getFullscreen()
  {
    return isFullscreen;
  }

private:
  static BOOL CALLBACK ChildWindows(HWND hwnd, LPARAM lp)
  {
    char *name = (char*)lp;
    char fname[1024] = {};
    if (GetWindowTextA(hwnd, fname, sizeof(fname)) > 0)
    {
      if (!string(fname).compare(name))
      {
        common.hwnd = hwnd;
        return false;
      }
    }
    return true;
  }
  static void WaitForWindowName(char *wname)
  {
    common.hwnd = NULL;
    while (EnumThreadWindows(common.tid, ChildWindows, (LPARAM)wname));
  }

  DWORD timeout;
  vector<SProcessData> pData;
  BOOL isFullscreen;
};

static TCreateProcess cProcess;

static LRESULT CALLBACK prclocalc(int code, WPARAM wp, LPARAM lp)
{
  if (code == HC_ACTION)
  {
    if (wp == WM_ACTIVATE && GetForegroundWindow() == cProcess.getProcessData()->at(0).mainHWND)
    {
      cout << "activate" << endl;
    }
  }
  return CallNextHookEx(common.hookc, code, wp, lp);
}

static BOOL isActiveL2AndNotOneFromCreatedWindows()
{
  HWND active = GetForegroundWindow();
  BOOL stat = false;
  for (size_t i = 0; i < cProcess.getProcessData()->size() && !stat; i++)
  {
  /*  if (active == cProcess.getProcessData()->at(i).mainHWND)
      stat = true;*/
  }
  if (!stat)
  {
    char buff[128] = {};
    GetWindowTextA(active, buff, 128);
    if (string(buff).compare(common.windowName))
      stat = true;
  }
  return !stat;
}

static BOOL screenL2Panel(HWND to, HWND from);
UINT setTimer(int msec);

static LRESULT CALLBACK prclocal(int code, WPARAM wp, LPARAM lp)
{
  if (code == HC_ACTION /*|| code == HC_NOREMOVE*/)
  if (wp == WM_KEYDOWN /*|| wp == WM_SYSKEYDOWN*/)
  {
    KBDLLHOOKSTRUCT *mes = (KBDLLHOOKSTRUCT*)lp;

    if (mes->vkCode == VK_SNAPSHOT)
    {
      screenL2Panel(common.hwnd_shortcut, cProcess.getProcessData()->at(0).mainHWND);
      return 1;
    }

    if (mes->vkCode >= VK_NUMPAD1 && mes->vkCode <= VK_NUMPAD9 && isActiveL2AndNotOneFromCreatedWindows())
    {
      DWORD key = mes->vkCode + (VK_F1 - VK_NUMPAD1);
      DWORD lpword = 0;
      INPUT input;
      input.ki.time = 0;
      input.ki.dwFlags = 0;
      input.ki.wScan = LOWORD(key);
      input.ki.wVk = LOWORD(key);
      input.type = INPUT_KEYBOARD;

      HWND active = GetForegroundWindow();

      for (size_t i = 0; i < cProcess.getProcessData()->size(); i++)
      {
        /*if (active != cProcess.getProcessData()->at(i).mainHWND)
          SetForegroundWindow(cProcess.getProcessData()->at(i).mainHWND);*/
        //UpdateWindow( cProcess.getProcessData()->at(i).mainHWND );
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(input));
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(input));
        //UpdateWindow( cProcess.getProcessData()->at(i).mainHWND );
      }

      SetForegroundWindow(active);
      //UpdateWindow( active );

      return 1;
    }
  }

  return CallNextHookEx(common.hook, code, wp, lp);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg)
  {
    case WM_CLOSE:
    {
                   if (hwnd == common.hwnd_shortcut)
                   {
                     DestroyWindow(common.hwnd_shortcut);
                     common.hwnd_shortcut = NULL;
                   }
                   else
                     PostMessageA(hwnd, WM_DESTROY, 0, 0);
                   return 0;
    }
    case WM_PAINT:
    {
                   PAINTSTRUCT ps;
                   HDC hdc = BeginPaint(hwnd, &ps);
                   HBRUSH hbr = CreateSolidBrush(RGB(~COLOR_BTNFACE, ~COLOR_BTNFACE, ~COLOR_BTNFACE));
                   SelectObject(hdc, hbr);
                   FillRect(hdc, &ps.rcPaint, hbr);
                   EndPaint(hwnd, &ps);
    }
      break;

    case WM_COMMAND:
    {
                     static BOOL isSetHook = false;
                     switch (wp)
                     {
                     case ID_ELEMS::ID_BTNHOOK:
                     {
                                                int stat = 0;
                                                if (isSetHook)
                                                {
                                                  stat = UnhookWindowsHookEx(common.hook);
                                                  isSetHook = false;
                                                  SetWindowTextA(common.btn_hook, "set hook");
                                                  SetWindowTextA(common.label_hook, stat ? "hook uninstalled" : "error uninstall hook");
                                                }
                                                else
                                                {
                                                  stat = (common.hook = SetWindowsHookExA(WH_KEYBOARD_LL, prclocal, NULL, 0)) ? 1 : 0;
                                                  isSetHook = true;
                                                  SetWindowTextA(common.btn_hook, "unset hook");
                                                  SetWindowTextA(common.label_hook, stat ? "hook installed" : "error install hook");
                                                }
                     }
                       break;

                     case ID_ELEMS::ID_BTNSTART:
                     {
                                                 char buff[32] = {};
                                                 SendMessageA(common.edit_start, WM_GETTEXT, sizeof(buff), (LPARAM)buff);
                                                 common.countWinFromEdit = atoi(buff);
                                                 cProcess.setTimeout(INFINITE);
                                                 if (cProcess.CreateProcess(common.pathToFile, common.windowName, common.countWinFromEdit))
                                                   cProcess.getFullscreen() ? cProcess.adjustAllSizesOnFullScreen() : false;
                     }
                       break;

                     case ID_ELEMS::ID_BTNSTOP:
                     {
                                                char buff[32] = {};
                                                if (GetWindowTextA(common.edit_stop, buff, sizeof(buff)))
                                                {
                                                  common.numWinForCloseFromEdit = atoi(buff) - 1;
                                                  if (common.numWinForCloseFromEdit < cProcess.getProcessData()->size())
                                                  {
                                                    PostMessageA(cProcess.getProcessData()->at(common.numWinForCloseFromEdit).mainHWND, WM_CLOSE, 0, 0);
                                                    cProcess.getProcessData()->erase(cProcess.getProcessData()->begin() + common.numWinForCloseFromEdit);
                                                  }
                                                }
                     }
                       break;

                     case ID_ELEMS::ID_SHOWHIDE:
                     {
                                                 if (cProcess.getProcessData()->size())
                                                 {
                                                   static bool flag = false;
                                                   cProcess.showAllWindows(flag);
                                                   flag = !flag;
                                                   SetWindowTextA(common.btn_showhide, flag ? "show all" : "hide all");
                                                 }
                     }
                       break;

                     case ID_ELEMS::ID_OPENFULLSCREEN:
                     {
                                                       SetWindowTextA(common.btn_fullscreen, cProcess.getFullscreen() ? "open fullscreen" : "no fullscreen");
                                                       cProcess.getFullscreen() ? cProcess.resetFullscreen() : cProcess.setFullscreen();
                     }
                       break;

                     case ID_ELEMS::ID_SHOWSHORTCUT:
                     {

                     }
                       break;

                     case ID_ELEMS::ID_TIMER:
                     {
                                              static bool startTimer = false;
                                              char buff[32] = {};
                                              if (GetWindowTextA(common.edit_timer, buff, sizeof(buff)) && atoi(buff) > 0)
                                              {
                                                if (!startTimer)
                                                  common.utimer = setTimer(atoi(buff));
                                                else
                                                  KillTimer(common.hwnd_shortcut, common.utimer);
                                                startTimer = !startTimer;
                                                SetWindowTextA(common.btn_timer, startTimer ? "stop timer" : "start timer");
                                              }
                     }
                       break;
                     }
    }
  }
  return DefWindowProcA(hwnd, msg, wp, lp);
}

static BOOL screenL2Panel(HWND to, HWND from)
{
  HDC hFrom = GetDC(from);
  HDC hTo = GetDC(to);

  int x = GetDeviceCaps(hTo, HORZRES);
  int y = GetDeviceCaps(hTo, VERTRES);

  POINT cur;
  GetCursorPos(&cur);

  BitBlt(hTo, 0, 0, x, y, hFrom, cur.x - 10, cur.y - 5, SRCCOPY);

  // clean up
  DeleteDC(hTo);
  DeleteDC(hFrom);

  return false;
}

void getScreen()
{
  HWND currentDesktop = GetForegroundWindow();
  HDC screenHDC = GetDC(currentDesktop);
  //HDC memoryHDC = CreateCompatibleDC(screenHDC);
  int screenWidth = GetDeviceCaps(screenHDC, HORZRES);
  int screenHeight = GetDeviceCaps(screenHDC, VERTRES);
  HBITMAP screenBITMAP = CreateCompatibleBitmap(screenHDC, screenWidth, screenHeight);
  HDC hdc = GetDC(currentDesktop);

  BITMAP bm;
  HDC hdcMem;
  POINT ptSize, ptOrg;

  hdcMem = CreateCompatibleDC(hdc);
  SelectObject(hdcMem, screenBITMAP);
  GetObject(screenBITMAP, sizeof(BITMAP), (LPVOID)&bm);

  ptSize.x = bm.bmWidth;
  ptSize.y = bm.bmHeight;
  ptOrg.x = 0;
  ptOrg.y = 0;

  //BitBlt( hdc, 0, 0, ptSize.x, ptSize.y, hdcMem, ptOrg.x, ptOrg.y, SRCCOPY );
  StretchBlt(hdc, 0, 0, ptSize.x, ptSize.y, hdcMem, ptOrg.x, ptOrg.y, ptSize.x, ptSize.y, SRCCOPY);
  DeleteDC(hdcMem);
}

void CALLBACK callTimer(HWND, UINT, UINT_PTR, DWORD)
{
  HWND active = GetForegroundWindow();
  for (vector<SProcessData>::iterator it = cProcess.getProcessData()->begin(); it != cProcess.getProcessData()->end(); ++it)
  {
    SendMessageA(it->mainHWND, WM_ACTIVATE, WA_ACTIVE, 0);
    //if (active != it->mainHWND)
      //SetForegroundWindow(it->mainHWND);
  }
  //SetForegroundWindow(active);
}

UINT setTimer(int msec)
{
  DWORD id = 0;
  SetTimer(common.hwnd_shortcut, id, msec, callTimer);
  return id;
}

static BOOL drawToCanvas(HWND hwnd, char* filename, int numberSkill = 0, int border = 0)
{
  IStream  *st;
  IPicture *pic;
  LRESULT rs = SHCreateStreamOnFileA(filename, STGM_SHARE_EXCLUSIVE, &st);
  if (rs == S_OK)
  {
    rs = OleLoadPicture(st, 0, true, IID_IPicture, (LPVOID*)&pic);
    if (rs == S_OK && pic)
    {
      BITMAP bmp;
      HBITMAP hbmp;
      pic->get_Handle((OLE_HANDLE*)&hbmp);
      hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, 25, 25, LR_COPYDELETEORG);
      GetObjectA(hbmp, sizeof(BITMAP), &bmp);
      if (hbmp && bmp.bmHeight > 0 && bmp.bmWidth > 0)
      {
        HDC dc = GetDC(hwnd);
        HDC memdc = CreateCompatibleDC(dc);
        SelectObject(memdc, hbmp);
        BitBlt(dc, numberSkill*bmp.bmWidth + border, 5, bmp.bmWidth, bmp.bmHeight, memdc, 0, 0, SRCCOPY);

        SelectObject(memdc, hbmp);
        DeleteObject(hbmp);
        DeleteDC(memdc);
        ReleaseDC(NULL, dc);
        DeleteDC(dc);
        st->Release();

        return true;
      }
    }
  }
  return false;
}

int CALLBACK WinMain(HINSTANCE hModule, HINSTANCE magicPeople, LPSTR param, int flags)
{
  WNDCLASSEXA wnd;
  wnd.cbSize = sizeof(WNDCLASSEXA);
  wnd.style = CS_HREDRAW | CS_VREDRAW;
  wnd.cbClsExtra = 0;
  wnd.cbWndExtra = 0;
  wnd.hbrBackground = 0;
  wnd.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_ARROW));
  wnd.hIcon = LoadIconA(NULL, MAKEINTRESOURCEA(IDI_APPLICATION));
  wnd.hIconSm = LoadIconA(NULL, MAKEINTRESOURCEA(IDI_APPLICATION));
  wnd.hInstance = hModule;
  wnd.lpfnWndProc = wndproc;
  wnd.lpszClassName = "_MYCLASS_";
  wnd.lpszMenuName = NULL;
  RegisterClassExA(&wnd);
  int x = GetSystemMetrics(SM_CXFULLSCREEN) / 2 - 265 / 2;
  int y = GetSystemMetrics(SM_CYFULLSCREEN) / 2 - 170 / 2;
  common.hwndMain = CreateWindowExA(WS_EX_APPWINDOW, wnd.lpszClassName, "l2 clicker", WS_SYSMENU | WS_MINIMIZEBOX,
    x, y, 265, 170, NULL, NULL, hModule, NULL);
  ShowWindow(common.hwndMain, SW_SHOW);

  using namespace ID_ELEMS;
  common.btn_start = CreateWindowA("button", "open", WS_CHILD, 10, 10, 100, 20, common.hwndMain, (HMENU)ID_BTNSTART, 0, 0);
  common.edit_start = CreateWindowA("edit", "1", WS_CHILD | WS_BORDER, 130, 10, 25, 20, common.hwndMain, (HMENU)ID_EDITSTART, 0, 0);
  common.btn_stop = CreateWindowA("button", "close", WS_CHILD, 10, 40, 100, 20, common.hwndMain, (HMENU)ID_BTNSTOP, 0, 0);
  common.edit_stop = CreateWindowA("edit", "1", WS_CHILD | WS_BORDER, 130, 40, 25, 20, common.hwndMain, (HMENU)ID_EDITSTOP, 0, 0);
  common.btn_hook = CreateWindowA("button", "set hook", WS_CHILD, 10, 70, 100, 20, common.hwndMain, (HMENU)ID_BTNHOOK, 0, 0);
  common.label_hook = CreateWindowA("static", "hook uninstalled", WS_CHILD, 130, 70, 150, 20, common.hwndMain, (HMENU)ID_LABEL, 0, 0);
  common.btn_showhide = CreateWindowA("button", "hide all", WS_CHILD, 10, 100, 100, 20, common.hwndMain, (HMENU)ID_SHOWHIDE, 0, 0);
  common.btn_fullscreen = CreateWindowA("button", "open fullscreen", WS_CHILD, 130, 100, 110, 20, common.hwndMain, (HMENU)ID_OPENFULLSCREEN, 0, 0);
  common.btn_timer = CreateWindowA("button", "start timer", WS_CHILD, 160, 10, 80, 20, common.hwndMain, (HMENU)ID_TIMER, 0, 0);
  common.edit_timer = CreateWindowA("edit", "1000", WS_CHILD | WS_BORDER, 170, 40, 60, 20, common.hwndMain, (HMENU)ID_EDITTIMER, 0, 0);
  ShowWindow(common.btn_start, SW_SHOW);
  ShowWindow(common.edit_start, SW_SHOW);
  ShowWindow(common.btn_stop, SW_SHOW);
  ShowWindow(common.edit_stop, SW_SHOW);
  ShowWindow(common.btn_hook, SW_SHOW);
  ShowWindow(common.label_hook, SW_SHOW);
  ShowWindow(common.btn_showhide, SW_SHOW);
  ShowWindow(common.btn_fullscreen, SW_SHOW);
  ShowWindow(common.edit_timer, SW_SHOW);
  ShowWindow(common.btn_timer, SW_SHOW);
  UpdateWindow(common.hwndMain);

  common.hwnd_shortcut = CreateWindowExA(WS_EX_TOPMOST, wnd.lpszClassName, "shortcuts", WS_SYSMENU | WS_MINIMIZEBOX,
    x, y, 335, 65, common.hwndMain, NULL, hModule, 0);
  common.label_panels = CreateWindowA("static", ">>", WS_CHILD, 0, 0, 1, 20, common.hwnd_shortcut, (HMENU)ID_LABELSHPORTCUT, 0, 0);
  ShowWindow(common.label_panels, SW_SHOW);
  //ShowWindow( common.hwnd_shortcut, SW_SHOW );
  UpdateWindow(common.hwnd_shortcut);
  //getScreen();

  //common.pathToFile = "c:\\windows\\system32\\notepad.exe";
  //common.windowName = "¡ÂÁ˚Ïˇ˚L˚È ÅE¡ÅEÅE˙C";
  //common.pathToFile = "e:\\l2c4\\system\\l2.exe";
  //common.windowName = "Lineage II";

  char l2path[1024] = {};
  GetCurrentDirectoryA(1024, l2path);
  strcpy(l2path, string(l2path).append("\\").append("l2.exe").c_str());

  if (GetFileAttributesA(l2path) != INVALID_FILE_ATTRIBUTES)
  {
    common.pathToFile = l2path;
    common.windowName = "Lineage II";
  }
  else
    MessageBoxA(common.hwndMain, "L2.exe not found!", "NOTIFY", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);

  MSG msg;
  while (GetMessageA(&msg, 0, 0, 0))
  {
    if (msg.message == WM_DESTROY)
    {
      DestroyWindow(common.hwndMain);
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  return 0;
}
