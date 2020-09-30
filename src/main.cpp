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
double frame_time = 30;

int lvl = 0;

// Util

double get_dur(clock_t then) {
  double result = (double)(clock() - then) / CLOCKS_PER_SEC;
  return result * 1000;
}

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
  bool left = false;
  int x, y;
  int x_spawn, y_spawn;
  int jumping = 0;

  void clear() {
    int pos = get_pos(x, y);
    select_length(pos, 1);
    replace({ map[pos], 0 });
  }
  
  void draw() {
    int pos = get_pos(x, y);
    select_length(pos, 1);
    replace({ left ? '<' : '>', 0 });
  }

  void move_to(int x, int y) {
    clear();
    if (this->x > x) left = true;
    if (this->x < x) left = false;
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
      if (get_block(x + i + (n < 0 ? -1 : 1), y) == 'X')
        return true;
    return false;
  }
  bool collision_y(int n) {
    if (y + n < 0 || y + n >= HEIGHT)
      return true;
    for (int i = 0; i != n; i += (n < 0 ? -1 : 1))
      if (get_block(x, y + i + (n < 0 ? -1 : 1)) == 'X')
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
  player.draw();
}

void load_level(int l) {
  lvl = l;
  map = read_map(l);
  redraw();
  for (int x = 0; x < WIDTH; x++) {
    for (int y = 0; y < HEIGHT; y++) {
      if (map[get_pos(x, y)] == 'S') {
        player.x_spawn = x;
        player.y_spawn = y;
        player.move_to(x, y);
        return;
      }
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

// Gameplay
  
clock_t jump_clock = clock();
int jump_height = 3;
double jump_time1 = 50;
double jump_time2 = 100;
int text_speed = 50;

void update_play(bool can_jump = true, int x_min = 0, int x_max = WIDTH - 1) {
  if (key_down(Key::Left) &&
      !player.collision_x(-1) &&
      player.x > x_min)
      player.move(-1, 0);
  if (key_down(Key::Right) &&
      !player.collision_x(1) &&
      player.x < x_max)
    player.move(+1, 0);

  if (key_pressed(Key::Jump) &&
      can_jump &&
      player.jumping == 0 &&
      !player.collision_y(-1)) {
    player.jumping = 1;
    player.move(0, -1);
    jump_clock = clock();
  }
  if (player.jumping != 0) {
    if (player.jumping < jump_height && get_dur(jump_clock) > jump_time1) {
      if (!player.collision_y(-1)) {
        player.move(0, -1);
        player.jumping++;
        jump_clock = clock();
      } else {
        player.jumping = jump_height;
      }
    } else if (player.jumping == jump_height && get_dur(jump_clock) > jump_time2) {
      player.jumping = 0;
    }
  }
  if (!player.jumping && !player.collision_y(1))
    player.move(0, +1);

  char b = get_block(player.x, player.y);
  if (b == '/' || b == '\\' || b == '<' || b == '>')
    player.move_to(player.x_spawn, player.y_spawn);
}

void print_text(int x, int y, string text, int delay) {
  for (int i = 0; i < text.size(); i++) {
    select_length(get_pos(x + i, y), 1);
    replace(text.substr(i, 1));
    Sleep(delay);
  }
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
    if (player.x == 17) {
      print_text(4, 4, "Jump with up.", text_speed);
      print_text(4, 6, "Stand on x.", text_speed);
      progress++;
    }
    break;
  case 2:
    update_play();
    if (player.x == 22) {
      print_text(4, 8, "Collect ? for ???.", text_speed);
      progress++;
    }
    break;
  case 3:
    update_play(true, 0, 33);
    if (get_block(player.x, player.y) == '?') {
      print_text(4, 10, "Avoid /\\.", text_speed);
      progress++;
    }
    break;
  case 4:
    update_play();
    if (player.x == 39) {
      print_text(4, 14, "Finish lvl by reaching O.", text_speed);
      progress++;
    }
    break;
  case 5:
    update_play();
    if (get_block(player.x, player.y) == 'O') {
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
    //dt = ((double)clock() - game_clock) / CLOCKS_PER_SEC * 1000;
    if (get_dur(game_clock) < frame_time) continue;
    game_clock = clock();
    update_key_state();

    if (key_pressed(Key::Exit))
      break;
    if (key_pressed(Key::Redraw))
      redraw();
    
    update_game();

    update_key_state_old();
  }
  close_notepad();

  return 0;
}