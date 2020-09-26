#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <fstream>
#include <stdio.h>
#include <string>
#include <time.h>
#include <windows.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#define CONSOLE

#ifdef CONSOLE
#pragma comment(linker, "/subsystem:console")
#else
#pragma comment(linker, "/subsystem:windows")
#endif

HWND hwnd = NULL;

STARTUPINFOA si;
PROCESS_INFORMATION pi;

const int WIDTH = 56, HEIGHT = 29;
int x = 0, y = 0;
int spawn_x, spawn_y;
bool right = true;

clock_t update_clock = clock();
double update_time = 40;

BYTE dikeys[256];
bool keys[4] = {false, false, false, false};
bool keys_old[4] = {false, false, false, false};

int jumping = 0;

clock_t jump_clock = clock();
double jump_time1 = 50;
double jump_time2 = 100;
int jump_height = 3;

DWORD wait_time = 10;

int text_speed = 2;

std::string map;

void press_down(WORD vk) {
  INPUT ip;

  ip.type = INPUT_KEYBOARD;
  ip.ki.wScan = 0;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;

  ip.ki.wVk = vk;
  ip.ki.dwFlags = 0;
  SendInput(1, &ip, sizeof(INPUT));
}

void press_up(WORD vk) {
  INPUT ip;
  ip.type = INPUT_KEYBOARD;
  ip.ki.wScan = 0;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;

  ip.ki.wVk = vk;
  ip.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &ip, sizeof(INPUT));
}

void press(WORD vk) {
  press_down(vk);
  press_up(vk);
}

void key_down(char c) {
  INPUT ip;
  ip.type = INPUT_KEYBOARD;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;

  ip.ki.dwFlags = KEYEVENTF_UNICODE;
  ip.ki.wVk = 0;
  ip.ki.wScan = c;
  SendInput(1, &ip, sizeof(INPUT));
}

void key_up(char c) {
  INPUT ip;
  ip.type = INPUT_KEYBOARD;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;

  // Release key
  ip.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
  SendInput(1, &ip, sizeof(INPUT));
}

void key(char c) {
  key_down(c);
  key_up(c);
}

LPDIRECTINPUT8 di;
LPDIRECTINPUTDEVICE8 keyboard;

HRESULT initializedirectinput8() {
  HRESULT hr;
  // Create a DirectInput device
  if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                                     IID_IDirectInput8, (VOID **)&di, NULL))) {
    return hr;
  }
  return 0;
}

void createdikeyboard() {
  di->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
  keyboard->SetDataFormat(&c_dfDIKeyboard);
  keyboard->SetCooperativeLevel(NULL, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
  keyboard->Acquire();
}

void destroydikeyboard() {
  keyboard->Unacquire();
  keyboard->Release();
}

#define keydown(name, key) (name[key] & 0x80)

std::string read_map(int lvl) {
  std::ifstream ifs("lvl/" + std::to_string(lvl) + ".txt",
                    std::ios::in | std::ios::binary);
  if (!ifs.good())
    puts("mist");
  ifs.seekg(0, std::ios::end);
  int length = ifs.tellg();
  ifs.seekg(0, std::ios::beg);
  char *buffer = new char[length + 1];
  ifs.read(buffer, length);
  ifs.close();
  buffer[length] = 0;
  std::string result(buffer);
  delete buffer;
  return result;
}

char get_block(int x, int y) {
  char result = map[(y + 1) * (WIDTH + 4) + x + 1];
  return result;
}

double get_dur(clock_t then) {
  double result = (double)(clock() - then) / CLOCKS_PER_SEC;
  return result * 1000;
}

void draw() {
  press(VK_BACK);
  key(right ? '>' : '<');
}

void erase() {
  press(VK_BACK);
  key(get_block(x, y));
}

void move(int dx, int dy) {
  erase();

  for (int i = 0; i < dx; i++) {
    right = true;
    press(VK_RIGHT);
  }
  for (int i = 0; i > dx; i--) {
    right = false;
    press(VK_LEFT);
  }
  for (int i = 0; i < dy; i++) {
    press(VK_DOWN);
  }
  for (int i = 0; i > dy; i--) {
    press(VK_UP);
  }

  draw();

  x += dx;
  y += dy;

  printf("%d %d\n", x, y);
}

void move_to(int _x, int _y) { move(_x - x, _y - y); }

void print_text(int text_x, int text_y, const char *text, int delay,
                bool move = true) {
  if (move) {
    for (int i = x; i < text_x; i++)
      press(VK_RIGHT);
    for (int i = x; i > text_x; i--)
      press(VK_LEFT);
    for (int i = y; i < text_y; i++)
      press(VK_DOWN);
    for (int i = y; i > text_y; i--)
      press(VK_UP);
  }

  int len = strlen(text);
  for (int i = 0; i < len; i++) {
    if (text[i] == '\r')
      continue;
    press(VK_DELETE);
    key(text[i]);
    if (move) {
      map[(text_y + 1) * (WIDTH + 4) + text_x + 1 + i] = text[i];
    }
    Sleep(delay);
  }
  if (move) {
    for (int i = text_x + len; i < x; i++)
      press(VK_RIGHT);
    for (int i = text_x + len; i > x; i--)
      press(VK_LEFT);
    for (int i = text_y; i < y; i++)
      press(VK_DOWN);
    for (int i = text_y; i > y; i--)
      press(VK_UP);
  }

  draw();

  Sleep(100);
}

void update_play(bool can_jump = true, int x_min = 0, int x_max = WIDTH - 1) {
  if (get_dur(update_clock) >= update_time) {
    update_clock = clock();

    if (keys[0] && x > x_min && get_block(x - 1, y) != 'X')
      move(-1, 0);
    if (keys[1] && x < x_max && get_block(x + 1, y) != 'X')
      move(+1, 0);
  }

  if (keys[2] && !keys_old[2] && jumping == 0 && can_jump) {
    jumping = 1;
    move(0, -1);
    jump_clock = clock();
  }
  if (jumping != 0) {
    if (jumping < jump_height && get_dur(jump_clock) > jump_time1) {
      if (get_block(x, y - 1) != 'X') {
        move(0, -1);
        jumping++;
        jump_clock = clock();
      } else {
        jumping = jump_height;
      }
    } else if (jumping == jump_height && get_dur(jump_clock) > jump_time2) {
      jumping = 0;
    }
  }
  if (!jumping && get_block(x, y + 1) != 'X' && y < HEIGHT - 1)
    move(0, +1);

  char b = get_block(x, y);
  if (b == '/' || b == '\\' || b == '<' || b == '>')
    move_to(spawn_x, spawn_y);
}

void redraw() {
  press_down(VK_CONTROL);
  press('A');
  press_up(VK_CONTROL);
  Sleep(100);
  press(VK_DELETE);
  Sleep(100);
  int _x = x, _y = y;
  print_text(0, 0, map.c_str(), 1, false);
  for (int i = 0; i < 100; i++)
    press(VK_UP);
  for (int i = 0; i < 100; i++)
    press(VK_LEFT);
  x = y = 0;
  press(VK_DOWN);
  press(VK_RIGHT);
  press(VK_RIGHT);
  move_to(_x, _y);
}

int lvl = 0;

void setup() {
  x = y = 0;
  press(VK_DOWN);
  press(VK_RIGHT);
  press(VK_RIGHT);

  for (int i = 0; i < WIDTH; i++)
    for (int j = 0; j < HEIGHT; j++)
      if (get_block(i, j) == 'S') {
        spawn_x = i;
        spawn_y = j;
      }
      
  move_to(spawn_x, spawn_y);
}

void load_level(int l, bool terminate = true) {
  lvl = l;
  map = read_map(lvl);
  if (terminate)
    TerminateProcess(pi.hProcess, 0);

  char cmd[100];
  sprintf(cmd, "notepad.exe lvl/%d.txt", lvl);

  if (!CreateProcessA(NULL,  // No module name (use command line)
                      cmd,   // Command line
                      NULL,  // Process handle not inheritable
                      NULL,  // Thread handle not inheritable
                      FALSE, // Set handle inheritance to FALSE
                      0,     // No creation flags
                      NULL,  // Use parent's environment block
                      NULL,  // Use parent's starting directory
                      &si,   // Pointer to STARTUPINFO structure
                      &pi)   // Pointer to PROCESS_INFORMATION structure
  ) {
    printf("CreateProcess failed (%d).\n", GetLastError());
    return;
  }

  Sleep(100);

  char title[100];
  sprintf(title, "%d.txt - Editor", lvl);
  hwnd = FindWindowA(NULL, title);

  SetWindowPos(hwnd, HWND_TOP, 100, 100, 960, 905, SWP_SHOWWINDOW);
  SetFocus(hwnd);

  for (int i = 0; i < 10; i++) {
    press_down(VK_CONTROL);
    press(VK_OEM_PLUS);
    press_up(VK_CONTROL);
  }

  setup();
}

void intro() {
  static int progress = 0;
  switch (progress) {
  case 0:
    print_text(4, 2, "Move with left/right.", text_speed);

    progress++;
    break;
  case 1:
    update_play(false);
    if (x == 17) {
      print_text(4, 4, "Jump with up.", text_speed);
      print_text(4, 6, "Stand on x.", text_speed);
      progress++;
    }
    break;
  case 2:
    update_play();
    if (x == 22) {
      print_text(4, 8, "Collect ? for ???.", text_speed);
      progress++;
    }
    break;
  case 3:
    update_play(true, 0, 33);
    if (get_block(x, y) == '?') {
      print_text(4, 10, "Avoid /\\.", text_speed);
      progress++;
    }
    break;
  case 4:
    update_play();
    if (x == 39) {
      print_text(4, 14, "Finish lvl by reaching O.", text_speed);
      progress++;
    }
    break;
  case 5:
    update_play();
    if (get_block(x, y) == 'O') {
      load_level(1);
    }
    break;
  }
}

void lvl1() {
  static int progress = 0;
  switch (progress) {
  case 0:
    print_text(4, 2, "Also avoid > and <.", text_speed);
    progress++;
    break;
  case 1:
    update_play();
    break;
  }
}

void update_game() {
  switch (lvl) {
  case 0:
    intro();
    break;
  case 1:
    lvl1();
    break;
  }
}

void enter_keys(std::string input, int delay) {
  for (int i = 0; i < input.size(); i++) {
    if (input[i] == 't') {
      press(VK_TAB);
      Sleep(delay);
    }
    else if (input[i] == 's') {
      press(VK_SPACE);
      Sleep(delay);
    }
    else if (input[i] == '~') {
      press_down(VK_LSHIFT);
      enter_keys(input.substr(i+1, 1), delay);
      press_up(VK_LSHIFT);
      i++;
    }
    else {
      int n = 0;
      int len = 0;
      while (input[i+len] >= '0' && input[i+len] <= '9') {
        n *= 10;
        n += input[i+len] - '0';
        len++;
      }
      for (int j = 0; j < n; j++) {
        if (input[i+len] == '~') {
          enter_keys(input.substr(i+len, 2), delay);
        }
        else {
          enter_keys(input.substr(i+len, 1), delay);
        }          
      }
      if (input[i+len] == '~')
        i++;
      i += len;
    }
  }
}

void toggle_key_repeat() {
  WinExec("c:\\windows\\system32\\control.exe /name Microsoft.EaseOfAccessCenter /page pageKeyboardEasierToUse", SW_NORMAL);
  Sleep(500);

  enter_keys("6ts9ts7~ts13ts5~tss6ts", 10);

  Sleep(100);

  press_down(VK_CONTROL);
  press('W');
  press_up(VK_CONTROL);

  Sleep(1000);
}

/*
  Todo:
  - Msg Box Intro
  - Multi Jump
  - more blocks/lvls
  - Set Accessibility 
  - Lua?
  - Scrolling?
*/
#ifdef CONSOLE
int main(int argc, char **argv) {
#else
int WinMain(HINSTANCE a0, HINSTANCE a1, LPSTR a2, int a3) {
#endif
  toggle_key_repeat();
  // Dies zu programmieren mit der reduzierten Inputrate.
  // Ist nicht angenehm. Ich werde es ändern.......

  // printf("%c", get_block(6, 23));
  // printf("%c", get_block(6, 24));

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  // MessageBoxA(NULL, "Guten Tag.", "Spiel Name???", MB_OK);

  HRESULT hr;
  initializedirectinput8();
  createdikeyboard();

  load_level(0, false);

  MSG Msg;
  while (true) {
    hr = keyboard->GetDeviceState(256, dikeys);
    if (keydown(dikeys, DIK_ESCAPE)) {
      TerminateProcess(pi.hProcess, 0);
      // MessageBoxA(NULL, "beendet...", "Schönes Wochenende.", MB_OK);
      break;
    }
    if (keydown(dikeys, DIK_R)) {
      redraw();
    }
    keys[0] = keydown(dikeys, DIK_LEFTARROW);
    keys[1] = keydown(dikeys, DIK_RIGHTARROW);
    keys[2] = keydown(dikeys, DIK_UPARROW);

    if (keys[0] && !keys_old[0])
      press(VK_RIGHT);
    if (keys[1] && !keys_old[1])
      press(VK_LEFT);
    if (keys[2] && !keys_old[2])
      press(VK_DOWN);

    update_game();

    keys_old[0] = keys[0];
    keys_old[1] = keys[1];
    keys_old[2] = keys[2];
    keys_old[3] = keys[3];

    WaitForSingleObject(pi.hProcess, wait_time);
  }

  toggle_key_repeat();

  destroydikeyboard();

  // Close process and thread handles.
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return 0;
}