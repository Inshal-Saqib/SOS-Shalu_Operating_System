# SOS — Shalu Operating System v1.0

> A custom 32-bit operating system built entirely from scratch using C and x86 Assembly language. SOS boots on real hardware or virtual machines, manages its own memory, reads hardware clocks, drives a PS/2 keyboard, and provides both a command-line shell and a graphical menu interface — all without any underlying operating system or standard library.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Boot Sequence](#boot-sequence)
- [Project Structure](#project-structure)
- [File Reference](#file-reference)
- [Command Reference](#command-reference)
- [Build Instructions](#build-instructions)
- [Default Credentials](#default-credentials)
- [Technologies Used](#technologies-used)
- [Architecture](#architecture)

---

## Overview

SOS (Shalu Operating System) is a bare-metal x86 operating system written in C and NASM Assembly. Every component — from the bootloader that runs before any OS exists, to the interactive shell that the user types commands into — was written from scratch with no external libraries or OS dependencies.

SOS demonstrates real-world implementation of core operating systems concepts:

- Hardware-level I/O port communication
- VGA text-mode graphics with a 200-line scroll buffer
- PS/2 keyboard input via polling
- Linked-list dynamic memory allocation (kmalloc / kfree)
- CMOS real-time clock reading via BCD conversion
- Interrupt-free boot sequence using GRUB Multiboot
- Password-protected login with lockout
- Dual-interface design: CLI shell + GUI desk mode

---

## Features

| Feature | Description |
|---|---|
| Boot Splash | Animated loading screen with progress bar |
| Secure Login | Username + password with 3-attempt lockout |
| Scrollable Terminal | 200-line history buffer, scroll with PgUp/PgDn |
| CLI Shell | Full command-line interface with prompt `sos>` |
| GUI Desk Mode | Text-mode graphical menu with arrow-key navigation |
| Memory Manager | Heap allocator with split and coalesce |
| Real-Time Clock | Hardware CMOS clock reading |
| Calendar | Monthly calendar with today highlighted |
| Command History | Logbook of last 32 commands |
| Clipboard | Ctrl+C to copy, Ctrl+V to paste |
| Calculator | Letter-operator arithmetic (a s m d r) |
| ASCII Banner | Large text art generator |
| System Dashboard | Full OS info, memory, uptime, drivers |
| Proper Shutdown | ACPI shutdown with shutdown screen |

---

## Boot Sequence

```
[ BIOS POST ]
      |
[ GRUB reads ISO, loads myos.bin at 1MB ]
      |
[ boot.asm: set up 16KB stack, jump to kernel_main() ]
      |
[ terminal_init() — VGA driver ready ]
      |
[ splash_show() — animated boot screen ]
      |
[ uptime_init() — record boot time from RTC ]
      |
[ auth_login() — username/password prompt ]
      |
[ memory_init() — heap allocator ready ]
      |
[ Shell loop: sos> prompt, readline, dispatch ]
```

---

## Project Structure

```
sos/
├── boot/
│   └── boot.asm              # Multiboot header, stack, entry point
├── kernel/
│   ├── kernel.c              # Kernel main, shell loop, all commands
│   ├── memory.c / .h         # Heap allocator (kmalloc / kfree)
│   ├── history.c / .h        # Command history + clipboard
│   ├── auth.c / .h           # Login screen and authentication
│   ├── shutdown.c / .h       # Proper shutdown and restart
│   ├── splash.c / .h         # Boot splash animation
│   ├── uptime.c / .h         # Runtime tracking via RTC diff
│   ├── sysinfo.c / .h        # System dashboard (status command)
│   ├── calc.c / .h           # Letter-operator calculator
│   ├── banner.c / .h         # ASCII art text generator
│   └── gui.c / .h            # GUI desk mode
├── drivers/
│   ├── vga.c / .h            # VGA text driver with scroll buffer
│   ├── keyboard.c / .h       # PS/2 keyboard polling driver
│   └── rtc.c / .h            # CMOS real-time clock driver
├── iso/
│   └── boot/grub/grub.cfg    # GRUB bootloader config
├── linker.ld                 # Linker script (kernel at 1MB)
├── Makefile                  # Full build pipeline
└── README.md                 # This file
```

---

## File Reference

### `boot/boot.asm`
The entry point for the entire OS. Written in NASM x86 Assembly.

- Defines the **Multiboot header** so GRUB can identify and load the kernel
- Allocates a **16 KB stack** in the `.bss` section
- Sets `ESP` to the top of the stack
- Calls `kernel_main()` — the C kernel entry point
- Contains an infinite `hlt` loop in case the kernel returns

### `kernel/kernel.c`
The core of SOS. Contains the shell loop, command dispatcher, and all command handlers.

**Key functions:**

| Function | Purpose |
|---|---|
| `kernel_main()` | Entry point — initializes all subsystems, starts shell loop |
| `readline()` | Reads keyboard input with backspace, tab, Ctrl+C/V, scroll keys |
| `run_command()` | Dispatches typed command to the correct handler |
| `print_banner()` | Prints the SOS welcome banner |
| `do_about()` | Handles `whoami` command |
| `do_memstat()` | Handles `memstat` command |
| `do_memcheck()` | Handles `memcheck` command |
| `do_logbook()` | Handles `logbook` command |
| `do_desk()` | Launches GUI mode loop |

### `kernel/memory.c`
A complete dynamic memory allocator over a 1MB static heap.

**Key functions:**

| Function | Signature | Purpose |
|---|---|---|
| `memory_init()` | `void memory_init(void)` | Sets up the heap with a single free block |
| `kmalloc()` | `void* kmalloc(size_t size)` | First-fit allocation with block splitting |
| `kfree()` | `void kfree(void* ptr)` | Frees block and coalesces adjacent free blocks |
| `memory_stats()` | `void memory_stats(size_t*, size_t*, size_t*)` | Returns used/free/total bytes |

Each block has a header containing a magic number (`0xDEADBEEF` free / `0xBEEFDEAD` used), size, and prev/next pointers.

### `kernel/history.c`
Stores command history in a circular ring buffer and provides a clipboard buffer.

**Key functions:**

| Function | Purpose |
|---|---|
| `history_add(line)` | Adds a command to the ring buffer (skips duplicates) |
| `history_get(offset, buf)` | Retrieves a past command by offset from end |
| `history_count_get()` | Returns total number of commands stored |
| `clipboard_copy(text, len)` | Copies text to the clipboard buffer |
| `clipboard_paste(buf)` | Pastes clipboard text into a buffer |

### `kernel/auth.c`
Provides a full-screen login interface with password masking and lockout.

**Key functions:**

| Function | Purpose |
|---|---|
| `auth_login()` | Shows login screen, reads username/password, validates |
| `read_input()` | Reads keyboard into buffer, masks with `*` if hidden |
| `draw_login()` | Renders the login screen with attempt counter |
| `draw_lockout()` | Renders the lockout screen and halts CPU |

Default credentials: `admin` / `sos123` — editable in `auth.c`.

### `kernel/shutdown.c`
Handles clean system shutdown with a dedicated shutdown screen.

**Key functions:**

| Function | Purpose |
|---|---|
| `shutdown()` | Disables interrupts, shows screen, tries ACPI/APM/VMware ports, halts |
| `restart()` | Shows restart screen, pulses keyboard controller reset line |

Shutdown tries three hardware ports: `0x604` (QEMU/Bochs), `0xB004` (APM), `0x4004` (VMware).

### `kernel/splash.c`
Animated boot splash screen shown before the login prompt.

**Key functions:**

| Function | Purpose |
|---|---|
| `splash_show()` | Renders logo, cycles through 10 boot messages, animates loading bar |
| `loading_bar()` | Draws a `[====   ] 60%` style progress bar |
| `wait()` | Busy-wait delay for animation timing |

### `kernel/uptime.c`
Tracks system uptime by comparing current RTC time against boot time.

**Key functions:**

| Function | Purpose |
|---|---|
| `uptime_init()` | Records boot hour/minute/second from RTC |
| `uptime_seconds()` | Returns elapsed seconds (handles midnight rollover) |
| `uptime_print()` | Prints uptime as `Xh Xm Xs` with boot time |

### `kernel/sysinfo.c`
Renders a full system information dashboard for the `status` command.

Displays: OS name, architecture, kernel type, bootloader, CPU info, heap memory (total/used/free), current date/time, uptime, and all active drivers.

### `kernel/calc.c`
A letter-operator expression calculator that avoids special characters.

**Operators:**

| Letter | Operation | Example |
|---|---|---|
| `a` | Addition | `5a3` = 8 |
| `s` | Subtraction | `9s4` = 5 |
| `m` | Multiplication | `6m7` = 42 |
| `d` | Division | `8d2` = 4 |
| `r` | Remainder (mod) | `9r4` = 1 |

Supports chaining: `2a3m4` evaluates left to right = 20.

**Key function:**

| Function | Purpose |
|---|---|
| `calc_run(expr)` | Parses and evaluates a letter-operator expression |

### `kernel/banner.c`
Prints text in large 5-row tall ASCII art letters using a built-in bitmap font.

Supports A–Z, 0–9, and space. Maximum 10 characters. Each row is printed in a different color.

**Key function:**

| Function | Purpose |
|---|---|
| `banner_print(text)` | Renders text as large colored ASCII art |

### `kernel/gui.c`
Text-mode graphical menu interface (the "desk" mode).

**Layout:** 2-column button grid, description box, clock in title bar, halt bar at bottom.

**Navigation:** Number keys 1–9, arrow keys (left/right switch columns, up/down move within column), Tab cycles buttons, Enter activates.

**Key functions:**

| Function | Purpose |
|---|---|
| `gui_run()` | Main GUI event loop — returns selected action |
| `gui_draw(sel)` | Renders entire GUI with selected button highlighted |
| `draw_btn(i, sel)` | Renders a single button with border and label |

### `drivers/vga.c`
Direct VGA text-mode driver with a 200-line scroll buffer.

Writes to VGA memory at `0xB8000`. Each cell is 2 bytes: ASCII character + color attribute byte.

**Key functions:**

| Function | Purpose |
|---|---|
| `terminal_init()` | Initializes buffer, clears screen |
| `terminal_putchar(c)` | Writes one character, handles `\n` and `\b` |
| `terminal_write(str)` | Writes a null-terminated string |
| `terminal_writeline(str)` | Writes string + newline |
| `terminal_setcolor(fg, bg)` | Sets current foreground/background color |
| `terminal_clear()` | Clears screen and resets scroll buffer |
| `terminal_erase_char()` | Backspace — removes last character from screen |
| `terminal_scroll_up(n)` | Scrolls view up by n lines |
| `terminal_scroll_down(n)` | Scrolls view down by n lines |
| `terminal_scroll_bottom()` | Snaps view to most recent output |

### `drivers/keyboard.c`
PS/2 keyboard driver using polling (no interrupts required).

Reads from port `0x60` when port `0x64` signals data is ready. Maps scancodes to ASCII characters.

**Key functions:**

| Function | Purpose |
|---|---|
| `keyboard_init()` | Initializes keyboard state |
| `keyboard_getchar()` | Blocks until a key is pressed, returns ASCII character |
| `keyboard_getchar_sc(sc)` | Converts a raw scancode to ASCII character |

### `drivers/rtc.c`
CMOS real-time clock driver. Reads hardware clock via I/O ports `0x70` (register select) and `0x71` (data).

All values are stored in BCD format by hardware and converted to binary integers before use.

**Key functions:**

| Function | Purpose |
|---|---|
| `rtc_read(t)` | Waits for update flag to clear, reads all time registers |
| `rtc_print_time()` | Prints `HH:MM:SS  DD/MM/YY` |
| `rtc_print_date()` | Prints full date with day name using Zeller's formula |
| `rtc_print_calendar()` | Renders full monthly calendar grid, today in green |
| `bcd_to_bin(bcd)` | Converts BCD byte to integer |
| `day_of_week(d, m, y)` | Returns 0–6 using Zeller's congruence algorithm |

### `linker.ld`
Custom GNU linker script. Places the kernel binary at physical address `0x100000` (1MB). Defines `.text`, `.rodata`, `.data`, and `.bss` sections aligned to 4KB boundaries. Sets `_start` as the entry point.

### `Makefile`
Full build pipeline:
1. Assembles `boot.asm` with NASM → `boot.o`
2. Compiles all `.c` files with GCC (`-m32 -ffreestanding -O2`)
3. Links all `.o` files with LD using `linker.ld` → `myos.bin`
4. Copies binary to ISO tree and runs `grub-mkrescue` → `myos.iso`

---

## Command Reference

| Command | Arguments | Description |
|---|---|---|
| `sos-help` | optional: `<command>` | Show all commands or detailed help for one |
| `wipe` | — | Clear the terminal screen |
| `whoami` | — | Show SOS version and feature list |
| `shutdown` | — | Proper system shutdown |
| `say` | `<text>` | Print text to terminal |
| `status` | — | Full system dashboard |
| `runtime` | — | Show system uptime |
| `memstat` | — | Memory usage (total/used/free) |
| `memcheck` | — | Run live memory allocator diagnostic |
| `time` | — | Current time and date |
| `date` | — | Monthly calendar |
| `compute` | `<expr>` | Calculator using letter operators |
| `splash` | `<text>` | Print text as ASCII art |
| `logbook` | — | View last 10 commands |
| `desk` | — | Open GUI desk mode |

### Calculator Operators

```
compute 5a3     →  5 + 3  = 8
compute 9s4     →  9 - 4  = 5
compute 6m7     →  6 × 7  = 42
compute 8d2     →  8 ÷ 2  = 4
compute 9r4     →  9 mod 4 = 1
compute 2a3m4   →  chained left-to-right = 20
```

### Keyboard Shortcuts

| Key | Action |
|---|---|
| `Backspace` | Delete last character |
| `Tab` | Insert 4 spaces |
| `Page Up` | Scroll terminal up 5 lines |
| `Page Down` | Scroll terminal down 5 lines |
| `↑ Arrow` | Scroll up 1 line |
| `↓ Arrow` | Scroll down 1 line |
| `Home` | Jump to top of terminal |
| `End` | Jump to bottom of terminal |
| `Ctrl+C` | Copy current typed line to clipboard |
| `Ctrl+V` | Paste clipboard into current line |

---

## Build Instructions

### Requirements (WSL2 Ubuntu or Linux)

```bash
sudo apt update && sudo apt install -y \
  nasm gcc grub-pc-bin grub-common \
  xorriso mtools qemu-system-x86 make
```

### Build

```bash
make clean && make
```

This produces `myos.iso` — a bootable ISO image.

### Run in QEMU

```bash
make run
```

### Run in VMware

1. Open VMware Workstation → New Virtual Machine
2. Select: Other → Other 32-bit
3. Settings → CD/DVD → Use ISO image → select `myos.iso`
4. Power On

### Access from Windows (WSL2)

Your ISO is at:
```
\\wsl$\Ubuntu\home\<username>\myos\myos.iso
```

---

## Default Credentials

| Field | Value |
|---|---|
| Username | `admin` |
| Password | `sos123` |
| Max attempts | 3 (then system locks) |

To change credentials, edit lines 10–11 in `kernel/auth.c`:
```c
#define VALID_USER "admin"
#define VALID_PASS "sos123"
```

---

## Technologies Used

| Technology | Role |
|---|---|
| C (GNU C99, -ffreestanding) | Kernel, drivers, all system logic |
| x86 NASM Assembly | Bootloader, stack setup, I/O instructions |
| GCC -m32 | Cross-compilation to 32-bit ELF |
| GNU LD + linker.ld | Custom linking at 1MB physical address |
| GRUB Multiboot | Bootloader — loads kernel from ISO |
| grub-mkrescue + xorriso | ISO image generation |
| VMware Workstation | Virtual machine for testing |
| QEMU | Rapid development testing |
| WSL2 Ubuntu | Build environment on Windows |

---

## Architecture

SOS uses a **monolithic kernel** architecture — all components run in kernel space at ring 0 with full hardware access. There is no user space, no system call interface, and no process isolation. This is appropriate for a single-purpose educational OS.

```
+--------------------------------------------------+
|              Application Layer                   |
|   Shell (CLI)    |    GUI Desk Mode              |
+--------------------------------------------------+
|              Kernel Layer                        |
|  Memory Mgr | History | Auth | Uptime | Sysinfo  |
+--------------------------------------------------+
|              Driver Layer                        |
|   VGA Driver | Keyboard Driver | RTC Driver      |
+--------------------------------------------------+
|              Hardware Layer                      |
|   x86 CPU | VGA 0xB8000 | PS/2 | CMOS | RAM     |
+--------------------------------------------------+
```

---

## License

Built as a final semester project for Operating Systems.
SOS — Shalu Operating System — 2025.
