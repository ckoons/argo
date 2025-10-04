# Building Argo-Term UI with Claude

© 2025 Casey Koons. All rights reserved.

## What Argo-Term Is

Argo-Term is an interactive terminal UI for visualizing and managing multi-AI workflows. It provides real-time monitoring, interactive control, and visual feedback for complex AI coordination tasks.

**Budget: 3,000 lines** (src/ only)

## Inherits Core Standards

Argo-Term follows all standards from `/CLAUDE.md` and `/arc/CLAUDE.md`:
- Memory management (ncurses requires careful cleanup)
- Error reporting (use `argo_report_error()`)
- No magic numbers (all constants in headers)
- File size limits (max 600 lines per file)

## UI-Specific Principles

### Responsive Design
- **Never block** - UI thread stays responsive always
- **Async updates** - Data fetches happen in background
- **Smooth refresh** - 30-60 FPS when active, slower when idle
- **Input priority** - User input processed before rendering

### Accessibility
- **Keyboard driven** - Every action has a key binding
- **Visual clarity** - Color enhances, never required
- **Graceful degradation** - Works on basic terminals
- **Screen reader hints** - Text mode fallback available

### Terminal Compatibility
```c
/* Support levels */
#define TERM_BASIC    0  /* 80x24, no color */
#define TERM_COLOR    1  /* Color support */
#define TERM_EXTENDED 2  /* 256 colors, Unicode */

/* Detect capability */
int term_level = detect_terminal();
if (term_level >= TERM_COLOR) {
    use_colors();
}
```

## ncurses Patterns

### Initialization
```c
#include <ncurses.h>

int ui_init(void) {
    initscr();              /* Initialize ncurses */
    cbreak();               /* Disable line buffering */
    noecho();               /* Don't echo input */
    keypad(stdscr, TRUE);   /* Enable function keys */
    curs_set(0);            /* Hide cursor */
    timeout(100);           /* Non-blocking input (100ms) */

    if (has_colors()) {
        start_color();
        init_color_pairs();
    }

    return ARGO_SUCCESS;
}

void ui_cleanup(void) {
    endwin();  /* CRITICAL: Always call on exit */
}
```

### Window Management
```c
/* Window structure */
typedef struct {
    WINDOW* win;
    int height;
    int width;
    int start_y;
    int start_x;
    char title[64];
} ui_panel_t;

/* Create panel */
ui_panel_t* panel_create(int h, int w, int y, int x, const char* title) {
    ui_panel_t* panel = calloc(1, sizeof(ui_panel_t));
    if (!panel) return NULL;

    panel->win = newwin(h, w, y, x);
    panel->height = h;
    panel->width = w;
    /* ... */

    box(panel->win, 0, 0);  /* Draw border */
    mvwprintw(panel->win, 0, 2, " %s ", title);

    return panel;
}
```

### Event Loop
```c
int ui_run(void) {
    int ch;
    while (!should_exit) {
        /* Process input */
        ch = getch();
        if (ch != ERR) {
            handle_input(ch);
        }

        /* Update data (non-blocking) */
        if (needs_update()) {
            fetch_workflow_status();
        }

        /* Render */
        ui_refresh();

        /* Sleep briefly to limit CPU */
        usleep(16666);  /* ~60 FPS */
    }

    return ARGO_SUCCESS;
}
```

## Layout Design

```
┌─────────────────────────────────────────────────────┐
│ Argo Multi-AI Workflow Monitor                      │
├─────────────────────────────────────────────────────┤
│ Workflows (3 active)          │  CI Registry (5)    │
│ ● build-123  [DEVELOP] 60%    │  ● builder  [BUSY]  │
│ ● test-456   [TEST]    100%   │  ● tester   [READY] │
│ ○ deploy-789 [PAUSED]  45%    │  ● reviewer [READY] │
│                                │  ○ deployer [OFFLINE]│
│ [r]efresh [p]ause [q]uit       │  ○ monitor  [ERROR] │
├─────────────────────────────────────────────────────┤
│ Messages (5 new)                                     │
│ [12:34] builder -> tester: "Tests ready for review" │
│ [12:35] tester -> reviewer: "Running unit tests..." │
│                                                      │
└─────────────────────────────────────────────────────┘
```

## Color Scheme

```c
/* Color pairs */
#define COLOR_PAIR_NORMAL    1
#define COLOR_PAIR_TITLE     2
#define COLOR_PAIR_SUCCESS   3
#define COLOR_PAIR_WARNING   4
#define COLOR_PAIR_ERROR     5
#define COLOR_PAIR_ACTIVE    6

void init_color_pairs(void) {
    init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_TITLE, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_PAIR_SUCCESS, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_WARNING, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_PAIR_ACTIVE, COLOR_BLACK, COLOR_WHITE);
}
```

## Key Bindings

```c
/* Standard bindings */
#define KEY_QUIT         'q'
#define KEY_REFRESH      'r'
#define KEY_PAUSE        'p'
#define KEY_HELP         'h'
#define KEY_NEXT_PANEL   '\t'     /* Tab */
#define KEY_PREV_PANEL   KEY_BTAB /* Shift-Tab */

/* Navigation */
#define KEY_UP_ALT       'k'  /* vi-style */
#define KEY_DOWN_ALT     'j'
#define KEY_LEFT_ALT     'h'
#define KEY_RIGHT_ALT    'l'
```

## State Management

```c
/* UI state */
typedef struct {
    ui_panel_t* workflow_panel;
    ui_panel_t* ci_panel;
    ui_panel_t* message_panel;

    int active_panel;
    int needs_refresh;
    int should_exit;

    /* Data from Argo */
    ci_registry_t* registry;
    workflow_controller_t* workflows[MAX_WORKFLOWS];
    int workflow_count;

    /* Update tracking */
    time_t last_update;
    int update_interval_ms;
} ui_state_t;
```

## Testing UI

```c
/* Mock terminal for testing */
void setup_test_ui(void) {
    /* Don't call initscr() in tests */
    /* Use string buffers instead */
}

void test_panel_create(void) {
    /* Test layout calculations */
    int h, w;
    getmaxyx(stdscr, h, w);

    ui_panel_t* panel = panel_create(h/2, w/2, 0, 0, "Test");
    assert(panel != NULL);
    assert(panel->height == h/2);

    panel_destroy(panel);
}
```

## Performance

- **Minimize redraws** - Only refresh changed regions
- **Batch updates** - Collect changes, render once
- **Efficient text** - Use `mvwaddstr()` not `mvwprintw()` for static text
- **Sleep appropriately** - Idle at 1 FPS, active at 60 FPS

## Error Handling

```c
/* ncurses errors aren't fatal but should be logged */
if (mvwprintw(win, y, x, text) == ERR) {
    /* Log but continue - may be off-screen */
    LOG_DEBUG("Failed to print at (%d,%d)", y, x);
}

/* Terminal resize */
void handle_resize(void) {
    endwin();
    refresh();
    clear();
    recreate_panels();
}
```

## Reminders

- **Always call `endwin()`** - Even on error paths
- **Check terminal size** - Don't assume 80x24
- **Test without color** - Should work in monochrome
- **Handle SIGWINCH** - Terminal resize signal
- **Keyboard only** - No mouse dependency

---

*Argo-Term: Visual clarity for complex AI workflows*
