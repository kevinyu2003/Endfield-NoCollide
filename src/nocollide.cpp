// Endfield No-Collide — lets playable characters walk through / stand inside each
// other (for photo mode) while leaving collision against enemies untouched.
//
// How Endfield resolves character collision (found by reading the game's IL2CPP
// code; it does NOT use UnityEngine.CharacterController or collider enable flags):
//
//   Beyond.Gameplay.Core.MovementComponent
//     CharacterCollisionSweep / Overlap / Raycast / SphereCollisionSweep
//         -> _CheckIfColliderValidForCollisions(ECSColliderResultProxy coll, bool ignoreInterEnemy)
//            (false = "ignore this collider", the capsule moves through it)
//     HandleInitialOverlap  (depenetration: pushes overlapping characters apart)
//         -> _CheckCollisionForPush(ECSColliderResultProxy coll, bool checkCantSwitch)
//            (false = "don't push away from this collider")
//
//   The colliders are ECS proxies, so we identify their owner with the game's own
//   static MovementComponent._GetCollisionOwnerEntityFromCollider(proxy) and compare
//   Entity.objectType. Both hooks return false ONLY when the moving entity AND the
//   collider's owner are ObjectType.Character. Enemy movement, enemy blocking,
//   hit boxes (BattleShape), projectiles and terrain are never touched.
//
// Loaded by the Endfield Poser style proxy (d3dcompiler_47.dll) from <game>\plugin\.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "MinHook.h"

#define NOCOL_VERSION "2.0.1"

// ---------------------------------------------------------------- logging / paths
static std::wstring g_dir;
static CRITICAL_SECTION g_logLock;
static FILE *g_log = nullptr;

static void Log(const char *fmt, ...) {
  if (!g_log) return;
  EnterCriticalSection(&g_logLock);
  SYSTEMTIME t; GetLocalTime(&t);
  fprintf(g_log, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
  va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
  fputc('\n', g_log); fflush(g_log);
  LeaveCriticalSection(&g_logLock);
}

static std::wstring ModuleDir(HMODULE m) {
  wchar_t p[MAX_PATH * 4] = {};
  DWORD n = GetModuleFileNameW(m, p, (DWORD)(sizeof(p) / sizeof(p[0])));
  std::wstring s(p, n);
  size_t k = s.find_last_of(L"\\/");
  return k == std::wstring::npos ? L"." : s.substr(0, k);
}

// ---------------------------------------------------------------- config
static std::atomic<bool> g_enabled{false};  // OFF at every launch unless config says enabled=1
static bool g_filterBlock = true;   // walking into each other
static bool g_filterPush = true;    // being pushed apart when overlapping
static int g_toggleVk = VK_F9;
static bool g_beep = true;

static const char *kDefaultConfig =
    "# Endfield No-Collide configuration\n"
    "# enabled: 0 = start OFF every launch (press the toggle key to turn on), 1 = start ON\n"
    "enabled=0\n"
    "# toggle_key: virtual-key code of the on/off hotkey (0x78 = F9). 0 disables the hotkey.\n"
    "toggle_key=0x78\n"
    "# ignore_block: 1 = characters no longer block each other while moving\n"
    "ignore_block=1\n"
    "# ignore_push: 1 = overlapping characters are no longer pushed apart\n"
    "ignore_push=1\n"
    "# beep: 1 = short sound when toggled (high = on, low = off)\n"
    "beep=1\n"
    "# Enemies are never affected: they still block you and still hit you.\n";

static void LoadConfig() {
  std::wstring path = g_dir + L"\\nocollide_config.txt";
  FILE *f = _wfopen(path.c_str(), L"rb");
  if (!f) {
    f = _wfopen(path.c_str(), L"wb");
    if (f) { fputs(kDefaultConfig, f); fclose(f); }
    Log("[CFG] wrote default config");
    return;
  }
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char *s = line;
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '#' || *s == ';' || !*s) continue;
    char *eq = strchr(s, '=');
    if (!eq) continue;
    *eq = 0;
    char *k = s, *v = eq + 1;
    for (char *e = eq - 1; e >= k && (*e == ' ' || *e == '\t'); --e) *e = 0;
    while (*v == ' ' || *v == '\t') ++v;
    long n = strtol(v, nullptr, 0);
    if (!_stricmp(k, "enabled")) g_enabled = n != 0;
    else if (!_stricmp(k, "toggle_key")) g_toggleVk = (int)n;
    else if (!_stricmp(k, "ignore_block")) g_filterBlock = n != 0;
    else if (!_stricmp(k, "ignore_push")) g_filterPush = n != 0;
    else if (!_stricmp(k, "beep")) g_beep = n != 0;
  }
  fclose(f);
  Log("[CFG] enabled=%d toggle_key=0x%X ignore_block=%d ignore_push=%d", (int)g_enabled.load(),
      g_toggleVk, g_filterBlock, g_filterPush);
}

// ---------------------------------------------------------------- il2cpp exports
#define IL2(ret, name, ...) typedef ret (*t_##name)(__VA_ARGS__); static t_##name name = nullptr
IL2(void *, il2cpp_domain_get);
IL2(void *, il2cpp_thread_attach, void *);
IL2(void *, il2cpp_thread_current);
IL2(void **, il2cpp_domain_get_assemblies, void *, size_t *);
IL2(void *, il2cpp_assembly_get_image, void *);
IL2(const char *, il2cpp_image_get_name, void *);
IL2(void *, il2cpp_class_from_name, void *, const char *, const char *);
IL2(void *, il2cpp_class_get_method_from_name, void *, const char *, int);
IL2(void *, il2cpp_class_get_field_from_name, void *, const char *);
IL2(size_t, il2cpp_field_get_offset, void *);
IL2(void, il2cpp_field_static_get_value, void *, void *);
IL2(void *, il2cpp_class_get_parent, void *);
IL2(void *, il2cpp_runtime_invoke, void *, void *, void **, void **);
#undef IL2

static bool ResolveExports(HMODULE ga) {
#define R(n) n = (t_##n)(void *)GetProcAddress(ga, #n)
  R(il2cpp_domain_get); R(il2cpp_thread_attach); R(il2cpp_thread_current);
  R(il2cpp_domain_get_assemblies); R(il2cpp_assembly_get_image); R(il2cpp_image_get_name);
  R(il2cpp_class_from_name); R(il2cpp_class_get_method_from_name);
  R(il2cpp_class_get_field_from_name); R(il2cpp_field_get_offset);
  R(il2cpp_field_static_get_value); R(il2cpp_class_get_parent); R(il2cpp_runtime_invoke);
#undef R
  return il2cpp_domain_get && il2cpp_thread_attach && il2cpp_thread_current &&
         il2cpp_domain_get_assemblies && il2cpp_assembly_get_image && il2cpp_image_get_name &&
         il2cpp_class_from_name && il2cpp_class_get_method_from_name &&
         il2cpp_class_get_field_from_name && il2cpp_field_get_offset && il2cpp_field_static_get_value &&
         il2cpp_class_get_parent && il2cpp_runtime_invoke;
}

// Runtime readiness: wait until the game itself completes a managed call on a
// registered thread (same approach Endfield Poser uses). Calling domain_get /
// thread_attach before that point can break the runtime start-up.
static t_il2cpp_runtime_invoke g_origInvoke = nullptr;
static std::atomic<bool> g_invokeSeen{false};
static void *BootInvoke(void *m, void *o, void **a, void **e) {
  void *r = g_origInvoke(m, o, a, e);
  if (!g_invokeSeen.load(std::memory_order_relaxed) && il2cpp_thread_current())
    g_invokeSeen.store(true, std::memory_order_release);
  return r;
}

static bool WaitRuntimeReady() {
  void *target = (void *)il2cpp_runtime_invoke;
  if (MH_CreateHook(target, (void *)BootInvoke, (void **)&g_origInvoke) != MH_OK ||
      MH_EnableHook(target) != MH_OK) {
    Log("[BOOT] could not observe runtime_invoke; falling back to a timed wait");
    Sleep(45000);
    return true;
  }
  ULONGLONG t0 = GetTickCount64();
  while (!g_invokeSeen.load(std::memory_order_acquire) && GetTickCount64() - t0 < 300000) Sleep(50);
  MH_DisableHook(target);  // keep the trampoline alive; just stop observing
  if (!g_invokeSeen) { Log("[BOOT] runtime never became ready (timeout)"); return false; }
  Log("[BOOT] runtime ready after %llu ms", GetTickCount64() - t0);
  return true;
}

// ---------------------------------------------------------------- game layout
static int OFF_COMP_ENTITY = 0x50;      // BaseComponent.<entity>k__BackingField
static int OFF_ENTITY_OBJTYPE = 0x5C;   // Entity.<objectType>k__BackingField
static int OBJTYPE_CHARACTER = 8;       // Beyond.Gameplay.Core.ObjectType.Character

// static Entity MovementComponent._GetCollisionOwnerEntityFromCollider(ECSColliderResultProxy)
// A 24-byte struct argument is passed by pointer on Windows x64.
typedef void *(*t_GetOwner)(const void *proxy, const void *method);
static t_GetOwner GetOwner = nullptr;
static void *GetOwnerMI = nullptr;

typedef bool (*t_Check)(void *self, const void *proxy, bool flag, const void *method);
static t_Check origValidForCollisions = nullptr;
static t_Check origCollisionForPush = nullptr;

static std::atomic<uint64_t> g_ignoredBlock{0}, g_ignoredPush{0};

static inline int EntityType(void *entity) {
  return entity ? *(int *)((char *)entity + OFF_ENTITY_OBJTYPE) : -1;
}

// True when a playable character's movement is testing against another
// playable character's collider.
static bool IsCharacterVsCharacter(void *self, const void *proxy) {
  if (!self || !proxy) return false;
  void *me = *(void **)((char *)self + OFF_COMP_ENTITY);
  if (EntityType(me) != OBJTYPE_CHARACTER) return false;  // enemies, NPCs... untouched
  void *other = GetOwner(proxy, GetOwnerMI);
  if (!other || other == me) return false;
  return EntityType(other) == OBJTYPE_CHARACTER;
}

static bool HookValidForCollisions(void *self, const void *proxy, bool ignoreInterEnemy, const void *mi) {
  if (g_filterBlock && g_enabled.load(std::memory_order_relaxed) && IsCharacterVsCharacter(self, proxy)) {
    if (g_ignoredBlock.fetch_add(1, std::memory_order_relaxed) == 0)
      Log("[RUN] first character-vs-character block ignored - mod is working");
    return false;
  }
  return origValidForCollisions(self, proxy, ignoreInterEnemy, mi);
}

static bool HookCollisionForPush(void *self, const void *proxy, bool checkCantSwitch, const void *mi) {
  if (g_filterPush && g_enabled.load(std::memory_order_relaxed) && IsCharacterVsCharacter(self, proxy)) {
    if (g_ignoredPush.fetch_add(1, std::memory_order_relaxed) == 0)
      Log("[RUN] first character-vs-character push ignored - mod is working");
    return false;
  }
  return origCollisionForPush(self, proxy, checkCantSwitch, mi);
}

// ---------------------------------------------------------------- resolution
static void *FindGameClass(const char *ns, const char *name) {
  size_t n = 0;
  void **asms = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &n);
  void *fallback = nullptr;
  for (size_t i = 0; i < n; i++) {
    void *img = il2cpp_assembly_get_image(asms[i]);
    if (!img) continue;
    void *k = il2cpp_class_from_name(img, ns, name);
    if (!k) continue;
    const char *in = il2cpp_image_get_name(img);
    if (in && !strcmp(in, "Gameplay.Beyond.dll")) return k;
    if (!fallback) fallback = k;
  }
  return fallback;
}

static int FieldOffsetInHierarchy(void *klass, const char *field) {
  for (int d = 0; klass && d < 12; d++, klass = il2cpp_class_get_parent(klass)) {
    void *f = il2cpp_class_get_field_from_name(klass, field);
    if (f) return (int)il2cpp_field_get_offset(f);
  }
  return -1;
}

static void *MethodPtr(void *mi) { return mi ? *(void **)mi : nullptr; }  // MethodInfo::methodPointer

static bool InstallHooks() {
  void *mc = FindGameClass("Beyond.Gameplay.Core", "MovementComponent");
  void *ent = FindGameClass("Beyond.Gameplay.Core", "Entity");
  void *ot = FindGameClass("Beyond.Gameplay.Core", "ObjectType");
  Log("[RES] MovementComponent=%p Entity=%p ObjectType=%p", mc, ent, ot);
  if (!mc || !ent) return false;

  int o = FieldOffsetInHierarchy(mc, "<entity>k__BackingField");
  if (o > 0) OFF_COMP_ENTITY = o;
  o = FieldOffsetInHierarchy(ent, "<objectType>k__BackingField");
  if (o > 0) OFF_ENTITY_OBJTYPE = o;
  if (ot) {
    void *f = il2cpp_class_get_field_from_name(ot, "Character");
    int v = -1;
    if (f) { il2cpp_field_static_get_value(f, &v); if (v > 0) OBJTYPE_CHARACTER = v; }
  }
  Log("[RES] offsets: component.entity=0x%X entity.objectType=0x%X ObjectType.Character=%d",
      OFF_COMP_ENTITY, OFF_ENTITY_OBJTYPE, OBJTYPE_CHARACTER);

  void *miOwner = il2cpp_class_get_method_from_name(mc, "_GetCollisionOwnerEntityFromCollider", 1);
  void *miValid = il2cpp_class_get_method_from_name(mc, "_CheckIfColliderValidForCollisions", 2);
  void *miPush = il2cpp_class_get_method_from_name(mc, "_CheckCollisionForPush", 2);
  GetOwner = (t_GetOwner)MethodPtr(miOwner);
  GetOwnerMI = miOwner;
  void *pValid = MethodPtr(miValid), *pPush = MethodPtr(miPush);
  Log("[RES] _GetCollisionOwnerEntityFromCollider=%p _CheckIfColliderValidForCollisions=%p _CheckCollisionForPush=%p",
      (void *)GetOwner, pValid, pPush);
  if (!GetOwner) { Log("[ERR] owner lookup not found - game update changed the code, mod inactive"); return false; }

  int ok = 0;
  if (pValid && MH_CreateHook(pValid, (void *)HookValidForCollisions, (void **)&origValidForCollisions) == MH_OK &&
      MH_EnableHook(pValid) == MH_OK) { ok++; Log("[OK] hooked _CheckIfColliderValidForCollisions"); }
  else Log("[ERR] could not hook _CheckIfColliderValidForCollisions");
  if (pPush && MH_CreateHook(pPush, (void *)HookCollisionForPush, (void **)&origCollisionForPush) == MH_OK &&
      MH_EnableHook(pPush) == MH_OK) { ok++; Log("[OK] hooked _CheckCollisionForPush"); }
  else Log("[ERR] could not hook _CheckCollisionForPush");
  return ok > 0;
}

// ---------------------------------------------------------------- hotkey
static bool GameHasFocus() {
  HWND w = GetForegroundWindow();
  DWORD pid = 0;
  if (w) GetWindowThreadProcessId(w, &pid);
  return pid == GetCurrentProcessId();
}

static DWORD WINAPI HotkeyThread(LPVOID) {
  bool was = false;
  ULONGLONG lastReport = GetTickCount64();
  for (;;) {
    Sleep(30);
    if (g_toggleVk > 0) {
      bool down = (GetAsyncKeyState(g_toggleVk) & 0x8000) != 0;
      if (down && !was && GameHasFocus()) {
        bool on = !g_enabled.load();
        g_enabled = on;
        Log("[KEY] no-collide %s", on ? "ON" : "OFF");
        if (g_beep) Beep(on ? 1200 : 500, 90);
      }
      was = down;
    }
    if (GetTickCount64() - lastReport > 600000) {  // light heartbeat every 10 min
      lastReport = GetTickCount64();
      Log("[RUN] enabled=%d ignored block=%llu push=%llu", (int)g_enabled.load(),
          (unsigned long long)g_ignoredBlock.load(), (unsigned long long)g_ignoredPush.load());
    }
  }
}

// ---------------------------------------------------------------- bootstrap
static DWORD WINAPI InitThread(LPVOID) {
  HMODULE ga = nullptr;
  for (int i = 0; i < 6000 && !(ga = GetModuleHandleW(L"GameAssembly.dll")); i++) Sleep(50);
  if (!ga) { Log("[BOOT] GameAssembly.dll never loaded"); return 0; }
  if (!ResolveExports(ga)) { Log("[BOOT] il2cpp exports missing"); return 0; }
  MH_STATUS s = MH_Initialize();
  if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) { Log("[BOOT] MinHook init failed %d", s); return 0; }
  if (!WaitRuntimeReady()) return 0;
  il2cpp_thread_attach(il2cpp_domain_get());  // this thread stays attached (it is idle afterwards)

  // Classes may not be registered yet on the first managed call; retry for a while.
  bool ok = false;
  for (int i = 0; i < 60 && !(ok = InstallHooks()); i++) Sleep(2000);
  if (!ok) { Log("[BOOT] giving up - no hooks installed"); return 0; }
  Log("[BOOT] ready. No-collide is %s. Press toggle key (0x%X) in game to switch.",
      g_enabled.load() ? "ON" : "OFF", g_toggleVk);
  CloseHandle(CreateThread(nullptr, 0, HotkeyThread, nullptr, 0, nullptr));
  return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(mod);
    // The plugin loader can also be pulled into helper processes from the game
    // folder; only run inside Endfield.exe so they don't overwrite the log.
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const wchar_t *base = wcsrchr(exe, L'\\');
    if (_wcsicmp(base ? base + 1 : exe, L"Endfield.exe") != 0) return TRUE;
    g_dir = ModuleDir(mod);
    InitializeCriticalSection(&g_logLock);
    g_log = _wfopen((g_dir + L"\\nocollide_log.txt").c_str(), L"w");  // fresh log each launch
    Log("=== Endfield No-Collide v" NOCOL_VERSION " loaded ===");
    LoadConfig();
    HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
  }
  return TRUE;
}
