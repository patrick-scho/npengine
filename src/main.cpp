#include <fstream>
#include <stdio.h>
#include <string>
#include <time.h>
#include <windows.h>
#include <Richedit.h>

#pragma comment(lib, "user32.lib")

#define CONSOLE

#ifdef CONSOLE
#pragma comment(linker, "/subsystem:console")
#else
#pragma comment(linker, "/subsystem:windows")
#endif

using namespace std;


const int WIDTH = 56, HEIGHT = 29;

std::string map;

clock_t game_clock;
double dt;

// Edit

HWND hwnd_notepad = NULL;
HWND hwnd_edit = NULL;

string get_text() {
  int len = SendMessageA(hwnd_edit, WM_GETTEXTLENGTH, 0, 0);
  char *buffer = new char[len + 1];
  int read = SendMessageA(hwnd_edit, WM_GETTEXT, len + 1, (LPARAM)buffer);
  if (read != len)
    puts("???");
  buffer[read] = 0;
  string result(buffer);
  delete[] buffer;
  return result;
}

void select_all() {
  SendMessage(hwnd_edit, EM_SETSEL, 0, get_text().size());
}

void select_length(int from, int length) {
  SendMessage(hwnd_edit, EM_SETSEL, from, from + length);
}

void select_from_to(int from, int to) {
  SendMessage(hwnd_edit, EM_SETSEL, from, to);
}

void replace(string str) {
  SendMessage(hwnd_edit, EM_REPLACESEL, FALSE, (LPARAM)str.c_str());
}

int get_pos(int x, int y) {
  return (y + 1) * (WIDTH + 4) + x + 1;
}

char get_block(int x, int y) {
  return map[get_pos(x, y)];
}

// Player
struct Player {
  int x_screen = -1, y_screen = -1;
  double x, y, x_vel, y_vel;

  void clear() {
    if (x_screen < 0 || y_screen < 0) return;
    int pos = get_pos(x_screen, y_screen);
    select_length(pos, 1);
    replace({ map[pos], 0 });
  }
  
  void draw() {
    if (collision_x(x - x_screen) || collision_y(y - y_screen)) {
      x = x_screen;
      y = y_screen;
      return;
    }
    x_screen = round(x);
    y_screen = round(y);
    int pos = get_pos(x_screen, y_screen);
    select_length(pos, 1);
    replace({ 'Q', 0 });
  }

  void update() {
    x += x_vel;
    y += y_vel;
    if (!collision_y(1) && y_vel < 9)
      y_vel += 1;
    if (abs(x - x_screen) >= 1 || abs(x - x_screen) >= 1) {
      clear();
      draw();
    }
  }

  void move_to(int x, int y) {
    clear();
    this->x = x;
    this->y = y;
    draw();
  }

  void move(int dx, int dy) {
    move_to(x + dx, y + dy);
  }

  bool collision_x(int n) {
    if (x + n < 0 || x + n >= WIDTH)
      return true;
    for (int i = 0; i != n; i += (n < 0 ? -1 : 1))
      if (get_block(x_screen + i + (n < 0 ? -1 : 1), y_screen) == 'X')
        return true;
    return false;
  }
  bool collision_y(int n) {
    if (y + n < 0 || y + n >= HEIGHT)
      return true;
    for (int i = 0; i != n; i += (n < 0 ? -1 : 1))
      if (get_block(x_screen, y_screen + i + (n < 0 ? -1 : 1)) == 'X')
        return true;
    return false;
  }
};
Player player { 0, 0 };

// Lvl

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

void redraw() {
  select_all();
  replace(map);
}

void load_level(int lvl) {
  map = read_map(lvl);
  redraw();
  for (int x = 0; x < WIDTH; x++) {
    for (int y = 0; y < HEIGHT; y++) {
      if (map[get_pos(x, y)] == 'S')
        player.move_to(x, y);
    }
  }
}

// Notepad

STARTUPINFOA si;
PROCESS_INFORMATION pi;

BOOL CALLBACK ew_cb(HWND hwnd, LPARAM lp) {
  DWORD pid;
  DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
  if (pi.dwProcessId == pid && pi.dwThreadId == tid) {
    hwnd_notepad = hwnd;
    return false;
  }
  return true;
}
BOOL CALLBACK ecw_cb(HWND child, LPARAM in) {
  char buffer[1024 + 1];
  int len = GetClassNameA(child, buffer, 1024);
  buffer[len] = 0;
  if (strcmp(buffer, "Edit") == 0) {
    hwnd_edit = child;
    return false;
  }
  return true;
}

void start_notepad() {
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  if (!CreateProcessA(NULL,  // No module name (use command line)
                      "notepad.exe",   // Command line
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
  }
  Sleep(100);
  EnumWindows(ew_cb, 0);
  EnumChildWindows(hwnd_notepad, ecw_cb, 0);
  SendMessage(hwnd_edit, EM_SETREADONLY, TRUE, NULL);
}
void close_notepad() {
  TerminateProcess(pi.hProcess, 0);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Keys

enum class Key {
  Left,
  Right,
  Jump,
  Exit,
  Redraw,
  COUNT
};

bool key_state[(uint64_t)Key::COUNT];
bool key_state_old[(uint64_t)Key::COUNT];

int key_get_vk(Key key) {
  switch (key) {
    case Key::Left: return 'A';
    case Key::Right: return 'D';
    case Key::Jump: return VK_SPACE;
    case Key::Exit: return VK_ESCAPE;
    case Key::Redraw: return 'R';
    default: return 0;
  }
}

void update_key_state() {
  for (int i = 0; i < (int)Key::COUNT; i++) {
    key_state[i] = GetAsyncKeyState(key_get_vk((Key)i));
  }
}

void update_key_state_old() {
  for (int i = 0; i < (int)Key::COUNT; i++) {
    key_state_old[i] = key_state[i];
  }
}

bool key_pressed(Key key) {
  return key_state[(int)key] && !key_state_old[(int)key];
}

bool key_down(Key key) {
  return key_state[(int)key];
}

bool key_up(Key key) {
  return !key_state[(int)key];
}

#ifdef CONSOLE
int main(int argc, char **argv) {
#else
int WinMain(HINSTANCE a0, HINSTANCE a1, LPSTR a2, int a3) {
#endif
  start_notepad();
  if (hwnd_notepad == NULL || hwnd_edit == NULL) {
    puts("error");
    return 1;
  }
  load_level(0);
  while (true) {
    dt = ((double)clock() - game_clock) / CLOCKS_PER_SEC * 1000;
    game_clock = clock();
    update_key_state();

    if (key_pressed(Key::Exit))
      break;
    if (key_pressed(Key::Redraw))
      redraw();
    if (key_down(Key::Left) && !player.collision_x(-1))
      player.move(-1, 0);
    if (key_down(Key::Right) && !player.collision_x(1))
      player.move(+1, 0);
    if (key_pressed(Key::Jump) && player.collision_y(1))
      player.y_vel = -5;

    player.update();

    printf("%f %f\n", player.x, player.y);

    update_key_state_old();
  }
  close_notepad();

  return 0;
}