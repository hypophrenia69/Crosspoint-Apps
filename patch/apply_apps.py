#!/usr/bin/env python3
"""Adds small apps (calculator, pomodoro timer) to a CrossPoint Reader source tree.

Usage: python3 apply_apps.py <path-to-crosspoint-reader> [owner/repo]

With owner/repo, the on-device "Check for updates" asks that GitHub repository's latest
release instead of the official one, so updates keep the apps.

The apps start from "/Apps/Calculator.txt" and "/Apps/Pomodoro.txt" in the file browser. The
patch only adds files plus three small hooks (start of an app, first-start files, update check);
it does not touch the reader, the home screen or the files other add-ons change.

Every change is anchored to a piece of the original code. If CrossPoint changes one of
those places, the script stops with a clear message instead of building something broken.
"""

import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
APP_FILES = ("AppsCore.h", "AppsActivity.h", "AppsActivity.cpp")


def fail(msg: str) -> None:
    print(f"APPS PATCH FAILED: {msg}", file=sys.stderr)
    sys.exit(1)


def replace_once(text: str, old: str, new: str, what: str) -> str:
    count = text.count(old)
    if count != 1:
        fail(f"'{what}': expected the anchor exactly once, found it {count} times. "
             f"CrossPoint probably changed this spot; the patch needs an update.")
    return text.replace(old, new, 1)


# Same text as in the other add-ons: only the first add-on that gets here adds it.
OTA_MARKER = "// Crosspoint add-ons: same CrossPoint version, newer add-on build number."
OTA_CODE = (
    '  ' + OTA_MARKER + '\n'
    '  // Add-on builds are named like "2.0.0-todo.12" or "2.0.0-dogear.3": a word after the dash,\n'
    '  // then the build number. Each add-on patch adds this check; only the first one does it.\n'
    '  {\n'
    '    const auto buildOf = [](const char* v) -> long {\n'
    '      const char* dash = strchr(v, \'-\');\n'
    '      if (dash == nullptr) return 0;\n'
    '      const char* dot = strrchr(dash, \'.\');\n'
    '      if (dot == nullptr || dot[1] < \'0\' || dot[1] > \'9\') return -1;\n'
    '      long n = 0;\n'
    '      for (const char* c = dot + 1; *c >= \'0\' && *c <= \'9\'; c++) n = n * 10 + (*c - \'0\');\n'
    '      return n;\n'
    '    };\n'
    '    const long latestBuild = buildOf(latestVersion.c_str());\n'
    '    if (latestBuild > 0) return latestBuild > buildOf(currentVersion);\n'
    '  }\n'
)


def patch_update_check(root: Path) -> None:
    """Check for Updates: a newer add-on build for the same CrossPoint version is an update too."""
    ota = root / "src" / "network" / "OtaUpdater.cpp"
    src = ota.read_text(encoding="utf-8")
    if OTA_MARKER in src:
        return  # another add-on did it already
    src = replace_once(src, '  if (strstr(currentVersion, "-rc") != nullptr) {\n',
                       OTA_CODE + '  if (strstr(currentVersion, "-rc") != nullptr) {\n', "update build number")
    ota.write_text(src, encoding="utf-8")


def edit(path: Path, edits) -> None:
    src = path.read_text(encoding="utf-8")
    for old, new, what in edits:
        src = replace_once(src, old, new, what)
    path.write_text(src, encoding="utf-8")


def patch_home(root: Path) -> None:
    manager_h = root / "src" / "activities" / "ActivityManager.h"
    home_h = root / "src" / "activities" / "home" / "HomeActivity.h"
    home_cpp = root / "src" / "activities" / "home" / "HomeActivity.cpp"
    grid_h = root / "src" / "components" / "CoverGridHomeUi.h"
    grid_cpp = root / "src" / "components" / "CoverGridHomeUi.cpp"
    for f in (manager_h, home_h, home_cpp, grid_h, grid_cpp):
        if not f.is_file():
            fail(f"{f} not found")

    edit(manager_h, [(
        "enum class HomeMenuItem { NONE, FILE_BROWSER, LIBRARY, OPDS_BROWSER, FILE_TRANSFER, SETTINGS_MENU };",
        "enum class HomeMenuItem { NONE, FILE_BROWSER, LIBRARY, OPDS_BROWSER, FILE_TRANSFER, APPS, SETTINGS_MENU };",
        "home menu item enum")])

    edit(home_h, [
        ("    if (item == HomeMenuItem::FILE_TRANSFER) return i;\n    ++i;\n"
         "    if (item == HomeMenuItem::SETTINGS_MENU) return i;\n",
         "    if (item == HomeMenuItem::FILE_TRANSFER) return i;\n    ++i;\n"
         "    if (item == HomeMenuItem::APPS) return i;  // Crosspoint-Apps\n    ++i;\n"
         "    if (item == HomeMenuItem::SETTINGS_MENU) return i;\n",
         "item to index"),
        ("    if (idx == i++) return HomeMenuItem::FILE_TRANSFER;\n    if (idx == i) return HomeMenuItem::SETTINGS_MENU;\n",
         "    if (idx == i++) return HomeMenuItem::FILE_TRANSFER;\n"
         "    if (idx == i++) return HomeMenuItem::APPS;  // Crosspoint-Apps\n"
         "    if (idx == i) return HomeMenuItem::SETTINGS_MENU;\n",
         "index to item"),
        ("  void onPluginsOpen();\n", "  void onPluginsOpen();\n  void onAppsOpen();  // Crosspoint-Apps\n", "apps handler"),
    ])

    edit(home_cpp, [
        ('#include "RecentBooksStore.h"\n',
         '#include "RecentBooksStore.h"\n#include "activities/apps/AppsActivity.h"\n', "home include"),
        ("  int count = 4;  // File Browser, Library, File transfer, Settings\n",
         "  int count = 5;  // File Browser, Library, File transfer, Apps (Crosspoint-Apps), Settings\n", "menu count"),
        ("      case HomeMenuItem::SETTINGS_MENU:\n        onSettingsOpen();\n        break;\n",
         "      case HomeMenuItem::APPS:  // Crosspoint-Apps\n        onAppsOpen();\n        break;\n"
         "      case HomeMenuItem::SETTINGS_MENU:\n        onSettingsOpen();\n        break;\n", "menu action"),
        ("  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_LIBRARY), tr(STR_FILE_TRANSFER),\n"
         "                                        tr(STR_SETTINGS_TITLE)};\n"
         "  std::vector<UIIcon> menuIcons = {Folder, Library, Transfer, Settings};\n",
         "  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_LIBRARY), tr(STR_FILE_TRANSFER), \"Apps\",\n"
         "                                        tr(STR_SETTINGS_TITLE)};  // Crosspoint-Apps: \"Apps\"\n"
         "  std::vector<UIIcon> menuIcons = {Folder, Library, Transfer, Blocks, Settings};\n", "menu entries"),
        ("void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }\n",
         "void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }\n"
         "void HomeActivity::onAppsOpen() { capps::openAppsMenu(renderer, mappedInput); }\n", "apps handler body"),
    ])

    edit(grid_h, [("std::array<freeink::ui::TabItem, 5> tabItems;", "std::array<freeink::ui::TabItem, 6> tabItems;",
                   "tab item count")])
    edit(grid_cpp, [
        ("static constexpr const uint8_t* ICONS[] = {FolderIcon, LibraryIcon, BlocksIcon, TransferIcon, Settings2Icon};\n"
         "  int count = 0;\n  for (int i = 0; i < 5; ++i) {\n",
         "static constexpr const uint8_t* ICONS[] = {FolderIcon,   LibraryIcon, BlocksIcon,\n"
         "                                              TransferIcon, BlocksIcon,  Settings2Icon};  // + Apps\n"
         "  int count = 0;\n  for (int i = 0; i < 6; ++i) {\n", "tab icons"),
    ])


def main() -> None:
    if len(sys.argv) not in (2, 3):
        fail("usage: apply_apps.py <crosspoint-reader folder> [owner/repo]")
    root = Path(sys.argv[1])
    manager = root / "src" / "activities" / "ActivityManager.cpp"
    main_cpp = root / "src" / "main.cpp"
    for f in (manager, main_cpp):
        if not f.is_file():
            fail(f"{f} not found")

    apps_dir = root / "src" / "activities" / "apps"
    if (apps_dir / "AppsActivity.cpp").exists():
        fail("patch is already applied to this tree")
    apps_dir.mkdir(parents=True, exist_ok=True)
    for name in APP_FILES:
        shutil.copy(HERE / "files" / name, apps_dir / name)

    # 1) A file in /Apps starts its app instead of the text reader.
    src = manager.read_text(encoding="utf-8")
    src = replace_once(src, '#include "util/BmpViewerActivity.h"\n',
                       '#include "apps/AppsActivity.h"\n#include "util/BmpViewerActivity.h"\n', "manager include")
    src = replace_once(
        src,
        "  if (FsHelpers::hasImageExtension(path)) {\n"
        "    auto activity = makeUniqueNoThrow<BmpViewerActivity>(renderer, mappedInput, std::move(path));\n",
        "  const capps::AppKind appKind = capps::appKind(path);  // Crosspoint-Apps\n"
        "  if (appKind != capps::AppKind::None) {\n"
        "    std::unique_ptr<Activity> app;\n"
        "    if (appKind == capps::AppKind::Calculator) {\n"
        "      app = makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput, std::move(path));\n"
        "    } else {\n"
        "      app = makeUniqueNoThrow<PomodoroActivity>(renderer, mappedInput, std::move(path));\n"
        "    }\n"
        "    if (!app) {\n"
        '      LOG_ERR("ACT", "OOM: app activity");\n'
        "      return;\n"
        "    }\n"
        "    replaceActivity(std::move(app));\n"
        "    return;\n"
        "  }\n"
        "\n"
        "  if (FsHelpers::hasImageExtension(path)) {\n"
        "    auto activity = makeUniqueNoThrow<BmpViewerActivity>(renderer, mappedInput, std::move(path));\n",
        "start apps")
    manager.write_text(src, encoding="utf-8")

    # 2) First start: the two start files in /Apps.
    src = main_cpp.read_text(encoding="utf-8")
    src = replace_once(src, '#include "SdCardFontSystem.h"\n',
                       '#include "SdCardFontSystem.h"\n#include "activities/apps/AppsActivity.h"\n', "main include")
    src = replace_once(src, '  LOG_DBG("MAIN", "Fonts setup");\n',
                       "  capps::ensureStarterApps();  // Crosspoint-Apps\n"
                       '  LOG_DBG("MAIN", "Fonts setup");\n', "starter files")
    main_cpp.write_text(src, encoding="utf-8")

    # 3) The "Apps" entry on the home screen, between File Transfer and Settings: in the list
    #    home and in the cover-grid home (a sixth icon in its tab bar).
    patch_home(root)

    # 4) "Check for Updates" also finds newer builds of the add-on.
    patch_update_check(root)

    # 5) On-device updates come from this repository's releases (they include the apps).
    if len(sys.argv) == 3:
        repo = sys.argv[2].strip()
        if repo.count("/") != 1 or not all(part for part in repo.split("/")):
            fail(f"'{repo}' is not in owner/repo form")
        ota = root / "src" / "network" / "OtaUpdater.cpp"
        ota_src = replace_once(ota.read_text(encoding="utf-8"),
                               '"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/latest"',
                               f'"https://api.github.com/repos/{repo}/releases/latest"', "update source")
        ota.write_text(ota_src, encoding="utf-8")
    print("Apps patch applied.")


if __name__ == "__main__":
    main()
