#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <fstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

HWND hwnd = NULL;

const int WIDTH = 43, HEIGHT = 25;
int x = 0, y = 0;
bool right = true;

clock_t update_clock = clock();
double update_time = 40;

bool keys[4] = { false, false, false, false };
bool keys_old[4] = { false, false, false, false };

int jumping = 0;

clock_t jump_clock = clock();
double jump_time1 = 50;
double jump_time2 = 100;
int jump_height = 3;

DWORD wait_time = 10;

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

  ip.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &ip, sizeof(INPUT));
}

void press(WORD vk) {
  press_down(vk);
  press_up(vk);
  GetAsyncKeyState(vk);
}

void key_down(char c) {
  if (hwnd == NULL)
    puts("oh no");
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
  if (hwnd == NULL)
    puts("oh no");
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

std::string read_map() {
  std::ifstream ifs("lvl/1.txt", std::ios::in | std::ios::binary);
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
std::string map = read_map();

char get_block(int x, int y) {
  char result = map[WIDTH + 8 + y * (WIDTH + 4) + x + 1];
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

void move_to(int _x, int _y) {
  move(_x - x, _y - y);
}

void print_text(int text_x, int text_y, const char *text, int delay) {
  for (int i = x; i < text_x; i++) press(VK_RIGHT);
  for (int i = x; i > text_x; i--) press(VK_LEFT);
  for (int i = y; i < text_y; i++) press(VK_DOWN);
  for (int i = y; i > text_y; i--) press(VK_UP);

  int len = strlen(text);
  for (int i = 0; i < len; i++) {
    press(VK_DELETE);
    key(text[i]);
    Sleep(delay);
  }
  for (int i = text_x + len; i < x; i++) press(VK_RIGHT);
  for (int i = text_x + len; i > x; i--) press(VK_LEFT);
  for (int i = text_y; i < y; i++) press(VK_DOWN);
  for (int i = text_y; i > y; i--) press(VK_UP);

  draw();

  Sleep(100);
}

enum GameState {
  GS_START, GS_INTRO1, GS_INTRO2
};

void update_play(bool can_jump = true) {
  if (get_dur(update_clock) >= update_time) {
    update_clock = clock();

    if (keys[0] &&
        x > 0 &&
        get_block(x - 1, y) != 'x')
      move(-1, 0);
    if (keys[1] &&
        x < WIDTH - 1 &&
        get_block(x + 1, y) != 'x')
      move(+1, 0);
  }

  // bool left = false;
  // bool right = false;
  // bool up = false;
  // bool down = false;

  if (keys[2] && !keys_old[2] && jumping == 0 && can_jump) {
    jumping = 1;
    move(0, -1);
    jump_clock = clock();
  }
  if (jumping != 0) {
    if (jumping < jump_height && get_dur(jump_clock) > jump_time1) {
      if (get_block(x, y - 1) != 'x') {
          move(0, -1);
          jumping++;
          jump_clock = clock();
      } else {
        jumping = jump_height;
      }
    }
    else if (jumping == jump_height && get_dur(jump_clock) > jump_time2) {
      jumping = 0;
    }
  }
  if (!jumping && get_block(x, y + 1) != 'x' && y < HEIGHT - 1)
      move(0, +1);

  char block = get_block(x, y);
  switch (block) {
  case '/':
  case '\\':
    move_to(0, 24);
    break;
  case '?':
    puts("?");
    break;
  case 'O':
    puts("O");
    break;
  }
}

enum GameState game_state = GS_START;
void update_game() {
  switch (game_state) {
  case GS_START:
    press(VK_DOWN);
    press(VK_RIGHT);
    press(VK_RIGHT);

    move_to(1, 24);

    print_text(4, 2, "Move with left/right.", 30);

    game_state = GS_INTRO1;
    break;
  case GS_INTRO1:
    update_play(false);
    if (x == 5) {
      print_text(4, 4, "Jump with up.", 30);
      game_state = GS_INTRO2;
    }
    break;
  case GS_INTRO2:
    update_play();
    break;
  }
}

/*
  Todo:
  - Restart
  - Next Level
*/
int main(int argc, char **argv) {
  // Dies zu programmieren mit der reduzierten Inputrate.
  // Ist nicht angenehm. Ich werde es ändern.......

  // printf("%c", get_block(6, 23));
  // printf("%c", get_block(6, 24));

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;

  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );
  
  //MessageBoxA(NULL, "Guten Tag.", "Spiel Name???", MB_OK);

  // Start the child process. 
  if( !CreateProcessA( NULL,   // No module name (use command line)
      "notepad.exe lvl/1.txt",        // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      0,              // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi )           // Pointer to PROCESS_INFORMATION structure
  ) 
  {
      printf( "CreateProcess failed (%d).\n", GetLastError() );
      return 0;
  }
  
  Sleep(100);

  hwnd = FindWindowA(NULL, "1.txt - Editor");

  HRESULT hr;
  BYTE dikeys[256];
  initializedirectinput8();
  createdikeyboard();

  MSG Msg;
	while (true) {
    hr = keyboard->GetDeviceState(256, dikeys);
    if (keydown(dikeys, DIK_ESCAPE)) {
      TerminateProcess(pi.hProcess, 0);
      //MessageBoxA(NULL, "beendet...", "Schönes Wochenende.", MB_OK);
			break;
		}
    keys[0] = keydown(dikeys, DIK_LEFTARROW);
    keys[1] = keydown(dikeys, DIK_RIGHTARROW);
    keys[2] = keydown(dikeys, DIK_UPARROW);

    if (keys[0] && !keys_old[0]) press(VK_RIGHT);
    if (keys[1] && !keys_old[1]) press(VK_LEFT);
    if (keys[2] && !keys_old[2]) press(VK_DOWN);

    update_game();

    keys_old[0] = keys[0];
    keys_old[1] = keys[1];
    keys_old[2] = keys[2];
    keys_old[3] = keys[3];

    WaitForSingleObject( pi.hProcess, wait_time);

    SetWindowPos(hwnd, HWND_TOPMOST, 100, 100, 750, 750, SWP_SHOWWINDOW);
  }

  destroydikeyboard();

  // Close process and thread handles. 
  CloseHandle( pi.hProcess );
  CloseHandle( pi.hThread );

  return 0;
}