# SOS — Shalu Operating System v1.0

> A custom 32-bit operating system built entirely from scratch in C and x86 Assembly. SOS boots on VMware or QEMU, manages its own memory, reads hardware clocks, drives a PS/2 keyboard, provides a CLI shell and GUI desk mode, and implements a full UDP network stack for VM-to-VM communication — all without any underlying OS or standard library.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Boot Sequence](#boot-sequence)
- [Project Structure](#project-structure)
- [File Reference](#file-reference)
- [Command Reference](#command-reference)
- [Network Communication](#network-communication)
- [Security System](#security-system)
- [Session Management](#session-management)
- [Power Management](#power-management)
- [Build Instructions](#build-instructions)
- [VMware Setup](#vmware-setup)
- [Default Credentials](#default-credentials)
- [System Requirements](#system-requirements)
- [Technologies Used](#technologies-used)
- [Architecture](#architecture)

---

## Overview

SOS (Shalu Operating System) is a bare-metal x86 OS written in C and NASM Assembly with zero external dependencies. Every component was written from scratch — from the bootloader, to the VGA driver, to the UDP network stack.

**What SOS demonstrates:**
- Hardware I/O port programming (VGA, PS/2, CMOS RTC, PCI, E1000 NIC)
- Dynamic memory management (linked-list heap allocator)
- Device drivers without OS support
- Secure authentication with lockout
- Dual UI: CLI shell + text-mode GUI
- Session persistence across logout
- Complete network stack: Ethernet + ARP + IPv4 + UDP
- VM-to-VM messaging between two SOS instances

---

## Features

| Feature | Description |
|---|---|
| Boot Splash | Animated 10-stage loading screen with progress bar |
| Secure Login | Username + password, 3-attempt lockout |
| Scrollable Terminal | 200-line history buffer, PgUp/PgDn/arrow scroll |
| CLI Shell | `sos>` prompt with full command set |
| GUI Desk Mode | Text-mode graphical menu, arrow-key navigation |
| Memory Manager | First-fit heap with block splitting and coalescing |
| Real-Time Clock | CMOS hardware clock, calendar with today highlighted |
| Command History | Last 32 commands in circular ring buffer |
| Clipboard | Ctrl+C to copy, Ctrl+V to paste |
| Calculator | Letter-operator arithmetic: a s m d r |
| ASCII Banner | 5-row tall text art generator (A-Z, 0-9) |
| System Dashboard | OS info, CPU, memory, uptime, drivers |
| Session Save | Logout saves context, restored on next login |
| Power Menu | S=Shutdown R=Restart L=Logout C=Cancel |
| Proper Shutdown | ACPI power-off with shutdown screen |
| Network Stack | E1000 NIC driver + ARP + IPv4 + UDP |
| VM Messaging | `send` / `recv` between two SOS VMs |

---

## Boot Sequence

```
[ BIOS POST ]
      │
[ GRUB reads ISO — loads myos.bin at physical address 1MB ]
      │
[ boot.asm — set up 16KB stack, call kernel_main() ]
      │
[ terminal_init() — VGA driver, 200-line scroll buffer ]
      │
[ splash_show() — animated boot screen with progress bar ]
      │
[ uptime_init() — record boot time from RTC ]
      │
[ memory_init() — heap allocator over 1MB static array ]
      │
[ keyboard_init() — PS/2 polling driver ]
      │
╔═══════════════════════════════╗
║     OUTER LOGIN LOOP          ║  ← Re-entered on logout
╚═══════════════════════════════╝
      │
[ auth_login() — username/password with masking ]
      │
[ session check — restore context if user logged out ]
      │
[ run_shell() — sos> prompt until logout requested ]
      │
[ shutdown() — S/R/L/C menu on power command ]
```

---

## Project Structure

```
SOS-Shalu_Operating_System/
├── .gitignore
├── README.md
├── Makefile
├── linker.ld                     # Kernel placed at 1MB
├── boot/
│   └── boot.asm                  # Multiboot header, 16KB stack, _start
├── kernel/
│   ├── kernel.c                  # Shell loop, all commands, kernel_main
│   ├── memory.c      memory.h    # Heap allocator (kmalloc/kfree)
│   ├── history.c     history.h   # Command history + clipboard
│   ├── auth.c        auth.h      # Login, password masking, lockout
│   ├── shutdown.c    shutdown.h  # Power menu S/R/L/C
│   ├── session.c     session.h   # Session save/restore on logout
│   ├── splash.c      splash.h    # Animated boot splash
│   ├── uptime.c      uptime.h    # Runtime tracking via RTC diff
│   ├── sysinfo.c     sysinfo.h   # System dashboard (status command)
│   ├── calc.c        calc.h      # Letter-operator calculator
│   ├── banner.c      banner.h    # ASCII art text generator
│   └── gui.c         gui.h       # GUI desk mode
├── drivers/
│   ├── vga.c         vga.h       # VGA text driver, 200-line scroll
│   ├── keyboard.c    keyboard.h  # PS/2 keyboard polling
│   ├── rtc.c         rtc.h       # CMOS real-time clock
│   └── net.c         net.h       # E1000 NIC + ARP + IPv4 + UDP
└── iso/
    └── boot/grub/
        └── grub.cfg              # GRUB config
```

---

## File Reference

### `boot/boot.asm`
Multiboot header, 16KB stack in `.bss`, `_start` entry point, calls `kernel_main()`. Contains infinite `hlt` loop as safety fallback.

### `kernel/kernel.c`
Core of SOS. Shell loop, command dispatcher, all handlers.

| Function | Purpose |
|---|---|
| `kernel_main()` | One-time init + outer login loop |
| `run_shell()` | Shell loop — reads commands until logout |
| `readline()` | Input with backspace, tab, Ctrl+C/V, scroll keys |
| `run_command()` | Dispatches base command to handler |
| `print_banner()` | SOS welcome banner |
| `print_resume_banner()` | Session restore info after logout |
| `do_about()` | `whoami` handler |
| `do_memstat()` | `memstat` handler |
| `do_memcheck()` | `memcheck` handler |
| `do_logbook()` | `logbook` handler |
| `do_desk()` | GUI mode loop |

### `kernel/memory.c`
1MB static heap, doubly-linked block list.

| Function | Purpose |
|---|---|
| `memory_init()` | Creates first free block (magic: DEADBEEF) |
| `kmalloc(size)` | First-fit allocation with block splitting |
| `kfree(ptr)` | Free + coalesce adjacent free blocks |
| `memory_stats()` | Returns used/free/total bytes |

### `kernel/history.c`
32-entry circular ring buffer + clipboard.

| Function | Purpose |
|---|---|
| `history_add(line)` | Adds command, skips duplicates |
| `history_get(offset, buf)` | Gets past command by offset |
| `clipboard_copy(text, len)` | Copies text to clipboard |
| `clipboard_paste(buf)` | Pastes clipboard to buffer |

### `kernel/auth.c`
Full-screen login with password masking and 3-attempt lockout.

| Function | Purpose |
|---|---|
| `auth_login()` | Login loop, returns on success |
| `read_input()` | Keyboard read, `*` masking for passwords |
| `draw_login()` | Login screen with attempt counter |
| `draw_lockout()` | Red lockout screen, halts CPU |

Credentials: `admin` / `sos123` (edit `auth.c` defines to change)

### `kernel/shutdown.c`
All power management with S/R/L/C confirmation menus.

| Function | Purpose |
|---|---|
| `shutdown()` | Shows CLI menu, acts on S/R/L/C choice |
| `shutdown_do()` | Direct shutdown (used by GUI after confirm) |
| `restart()` | Restart screen + keyboard controller CPU reset |
| `logout_sos()` | Logout screen + sets `g_logout_requested=1` |
| `shutdown_confirm_cli()` | Text menu, reads S/R/L/C scancode |
| `shutdown_confirm_gui()` | VGA overlay box, reads S/R/L/C scancode |

ACPI ports tried in order: `0x604` (QEMU), `0xB004` (Bochs), `0x4004` (VirtualBox), VMware backdoor `0x5658`.

### `kernel/session.c`
Saves shell context across logout without rebooting.

| Function | Purpose |
|---|---|
| `session_save(cmd, count, user)` | Saves last command, count, username |
| `session_load(out)` | Copies saved session to output struct |
| `session_exists()` | Returns 1 if valid session present |
| `session_clear()` | Clears session after restore |

### `kernel/splash.c`
Animated boot splash with logo and 10-stage loading bar.

### `kernel/uptime.c`
Tracks uptime by comparing RTC time against recorded boot time. Handles midnight rollover.

### `kernel/sysinfo.c`
Full 76-column dashboard: OS, CPU, heap stats, date/time, uptime, all drivers.

### `kernel/calc.c`
Letter-operator calculator. Parses left-to-right, supports chaining, detects division by zero.

### `kernel/banner.c`
5-row ASCII art from built-in bitmap font (A-Z, 0-9). Max 10 characters.

### `kernel/gui.c`
Text-mode GUI with 2×4 button grid, live clock, description box.

| Function | Purpose |
|---|---|
| `gui_run()` | Event loop, returns selected action |
| `gui_draw(sel)` | Renders full GUI |
| `draw_btn(i, sel)` | Renders single button with border |

Navigation: number keys 1-9, arrow keys, Tab, Enter.

### `drivers/vga.c`
Direct VGA text-mode driver at `0xB8000`. 200-line software scroll buffer.

| Function | Purpose |
|---|---|
| `terminal_init()` | Clear buffer, reset cursor |
| `terminal_putchar(c)` | Write char, handles `\n` `\b` auto-scroll |
| `terminal_write(str)` | Write null-terminated string |
| `terminal_writeline(str)` | Write string + newline |
| `terminal_setcolor(fg, bg)` | Set VGA color attribute |
| `terminal_erase_char()` | Proper backspace — erases from screen |
| `terminal_scroll_up(n)` | Scroll viewport up n lines |
| `terminal_scroll_down(n)` | Scroll viewport down n lines |
| `terminal_scroll_bottom()` | Snap to newest output |

### `drivers/keyboard.c`
PS/2 polling driver. Reads port `0x64` status, gets scancode from `0x60`.

| Function | Purpose |
|---|---|
| `keyboard_init()` | Initialize state |
| `keyboard_getchar()` | Block until key pressed, return ASCII |
| `keyboard_getchar_sc(sc)` | Convert raw scancode to ASCII |

### `drivers/rtc.c`
CMOS clock via ports `0x70`/`0x71`. BCD to binary conversion.

| Function | Purpose |
|---|---|
| `rtc_read(t)` | Wait for update flag, read all registers |
| `rtc_print_time()` | Print `HH:MM:SS DD/MM/YY` |
| `rtc_print_date()` | Full date with day name (Zeller's formula) |
| `rtc_print_calendar()` | Monthly grid, today in green |
| `bcd_to_bin(bcd)` | `(bcd & 0x0F) + (bcd >> 4) * 10` |
| `day_of_week(d,m,y)` | Zeller's congruence, returns 0-6 |

### `drivers/net.c`
Complete network stack targeting Intel E1000 (VMware default NIC).

| Function | Purpose |
|---|---|
| `net_start(vm_num)` | Init NIC, assign IP/peer — `1`=VM1 `2`=VM2 |
| `net_send(dst_ip, msg)` | Build Ethernet+IP+UDP frame, transmit |
| `net_poll(buf, max)` | Check RX ring for UDP packet on port 5000 |
| `find_e1000()` | PCI bus scan for Intel E1000 device IDs |
| `e1000_setup()` | Reset NIC, configure TX/RX descriptor rings |
| `build_frame()` | Construct complete Ethernet+IP+UDP frame |
| `net_parse_ip(str)` | Parse `"a.b.c.d"` to `uint32_t` |
| `net_print_ip(ip)` | Print `uint32_t` as `"a.b.c.d"` |

---

## Command Reference

| Command | Syntax | Description |
|---|---|---|
| `sos-help` | `sos-help [cmd]` | Full help or details for one command |
| `wipe` | `wipe` | Clear screen and redraw banner |
| `whoami` | `whoami` | SOS version, build info, feature list |
| `shutdown` | `shutdown` | Power menu: S=Shutdown R=Restart L=Logout C=Cancel |
| `say` | `say <text>` | Print any text to terminal |
| `status` | `status` | Full system dashboard |
| `runtime` | `runtime` | System uptime in h/m/s |
| `memstat` | `memstat` | Heap memory total/used/free |
| `memcheck` | `memcheck` | Live allocator diagnostic test |
| `time` | `time` | Current time and date from CMOS |
| `date` | `date` | Monthly calendar, today in green |
| `compute` | `compute <expr>` | Calculator with letter operators |
| `splash` | `splash <text>` | ASCII art text (max 10 chars) |
| `logbook` | `logbook` | Last 10 typed commands |
| `desk` | `desk` | Open GUI desk mode |
| `netstart` | `netstart 1` or `netstart 2` | Init NIC and assign VM identity |
| `send` | `send <message>` | Send UDP message to peer VM |
| `recv` | `recv` | Wait up to 120s for message (C=cancel) |
| `netstat` | `netstat` | Show network status |

### Calculator Operators
```
compute 5a3      →  5 + 3  = 8
compute 9s4      →  9 - 4  = 5
compute 6m7      →  6 × 7  = 42
compute 8d2      →  8 ÷ 2  = 4
compute 9r4      →  9 mod 4 = 1
compute 2a3m4    →  left-to-right = 20
```

### Keyboard Shortcuts
| Key | Action |
|---|---|
| `Backspace` | Delete last character |
| `Tab` | Insert 4 spaces |
| `Page Up` | Scroll up 5 lines |
| `Page Down` | Scroll down 5 lines |
| `↑ / ↓ Arrow` | Scroll up/down 1 line |
| `Home` | Jump to top of terminal |
| `End` | Jump to bottom |
| `Ctrl+C` | Copy current typed line |
| `Ctrl+V` | Paste clipboard |

### Power Menu Options
| Key | Action |
|---|---|
| `S` | Shutdown — ACPI power off, VM powers down |
| `R` | Restart — reboot through splash → login |
| `L` | Logout — save session, return to login |
| `C` | Cancel — return to shell |

---

## Network Communication

SOS implements a full 4-layer network stack from scratch.

### Protocol Stack
```
Application  →  message string
UDP          →  port 5000, no handshake
IPv4         →  src/dst IP, TTL=64, checksum
ARP          →  IP to MAC resolution (broadcast + reply)
Ethernet II  →  MAC framing, EtherType 0x0800
E1000 DMA    →  TX/RX descriptor rings, PCI BAR0 MMIO
VMnet        →  Host-Only virtual switch between VMs
```

### VM Setup (Two VMs on same laptop)
1. **Clone your VM:** Right-click VM → Manage → Clone → Full Clone → name it `SOS-VM2`
2. **Both VMs:** Settings → Network Adapter → **Host-Only**
3. **Boot both VMs** and login

### Usage
**VM1 — listen first:**
```
sos> netstart 1
  [NET] Ready!  My  IP : 192.168.100.10
  [NET] Ready!  Peer IP: 192.168.100.20
sos> recv
  Time left: 120s  [press C to cancel]
```

**VM2 — send message:**
```
sos> netstart 2
  [NET] Ready!  My  IP : 192.168.100.20
  [NET] Ready!  Peer IP: 192.168.100.10
sos> send Hello from VM2!
  [NET] Sending to peer (192.168.100.10): Hello from VM2!
  [NET] Sent!
```

**VM1 receives:**
```
  [NET] Message received: Hello from VM2!
```

### Hardcoded Network Config
```
VM1 IP  : 192.168.100.10
VM2 IP  : 192.168.100.20
VM1 MAC : 00:0C:29:19:37:98  (from VMware .vmx)
VM2 MAC : FF:FF:FF:FF:FF:FF  (broadcast until ARP resolves)
Port    : 5000 (UDP)
```
> To update VM1 MAC: edit `VM1_MAC` in `drivers/net.c` line 8 with your `.vmx` `generatedAddress` value.

---

## Security System

| Feature | Detail |
|---|---|
| Login screen | Full-screen, renders before shell starts |
| Password masking | `*` characters shown instead of actual password |
| Attempt limit | 3 attempts before lockout |
| Lockout screen | Red full-screen, CPU halted, press R to restart |
| Credentials | `#define VALID_USER` and `#define VALID_PASS` in `auth.c` |

---

## Session Management

On logout (`L` from power menu), SOS saves your session to RAM:
- Last command typed
- Total commands run this session
- Username

On next login, SOS detects the saved session and shows:
```
  +------------------------------------------+
  |       Session Restored Successfully       |
  +------------------------------------------+
  Welcome back, admin
  Commands run this session: 14
  Last command: memstat
```

Session is saved after **every command** automatically. Lost on shutdown or restart.

---

## Power Management

Type `shutdown` from shell or press button 9 in GUI desk mode:

```
  +----------------------------------------------+
  |          SOS  Power  Management              |
  +----------------------------------------------+
  |  S  =  Shutdown   (power off completely)    |
  |  R  =  Restart    (reboot to login screen)  |
  |  L  =  Log out    (save & return to login)  |
  |  C  =  Cancel     (return to shell)         |
  +----------------------------------------------+
  Press S / R / L / C :
```

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
Produces `myos.iso`.

### Run in QEMU
```bash
make run
```

### Access ISO from Windows (WSL2)
```
\\wsl$\Ubuntu\home\<username>\myos\myos.iso
```

---

## VMware Setup

### Single VM
1. New VM → Typical → I will install OS later
2. Other → Other 32-bit
3. Settings → CD/DVD → Use ISO → select `myos.iso`
4. Power On

### Two VMs for Network
1. Create first VM as above
2. Right-click VM → Manage → Clone → Full Clone → `SOS-VM2`
3. Both VMs: Settings → Network Adapter → **Host-Only**
4. Boot both, run `netstart 1` and `netstart 2`

---

## Default Credentials

| Field | Value |
|---|---|
| Username | `admin` |
| Password | `123` |
| Max attempts | 3 |
| Change in | `kernel/auth.c` — `VALID_USER` and `VALID_PASS` |

---

## System Requirements

| Component | Requirement |
|---|---|
| CPU | x86 32-bit (i386 compatible) |
| RAM | Minimum 5.2 MB, recommended 8 MB |
| Storage | CD-ROM/ISO only — no hard disk needed |
| Display | VGA text mode (80×25) |
| Keyboard | PS/2 (or USB with BIOS PS/2 emulation) |
| Network | Intel E1000 virtual NIC (VMware default) |
| Boot | BIOS/CSM — UEFI not supported |

---

## Technologies Used

| Technology | Role |
|---|---|
| C (GNU C99, -ffreestanding) | All kernel, driver, and system logic |
| x86 NASM Assembly | Bootloader, stack, I/O port access |
| GCC -m32 | 32-bit ELF cross-compilation |
| GNU LD + linker.ld | Custom linking at 1MB physical address |
| GRUB Multiboot | Bootloader — loads kernel from ISO |
| grub-mkrescue + xorriso | ISO image generation |
| VMware Workstation Pro | Primary VM platform |
| QEMU | Development testing |
| WSL2 Ubuntu | Build environment on Windows |
| Intel E1000 NIC | Virtual NIC for network communication |

---

## Architecture

```
+================================================================+
|                      Shell Layer                               |
|    CLI (sos> prompt)          |       GUI Desk Mode            |
+================================================================+
|                      Kernel Layer                              |
|  Memory | History | Auth | Session | Shutdown | Uptime        |
|  Sysinfo | Calc | Banner | GUI | Network Commands             |
+================================================================+
|                      Driver Layer                              |
|   VGA Driver  |  Keyboard Driver  |  RTC Driver  |  E1000 NIC |
+================================================================+
|                      Hardware Layer                            |
|  x86 CPU | VGA 0xB8000 | PS/2 | CMOS RTC | E1000 | RAM       |
+================================================================+

Network Stack:
┌───────────────────────────────┐
│  Application  — message str   │  send/recv commands
├───────────────────────────────┤
│  UDP          — port 5000     │  no handshake needed
├───────────────────────────────┤
│  IPv4         — IP + checksum │  TTL=64, protocol=UDP
├───────────────────────────────┤
│  ARP          — IP→MAC        │  broadcast + unicast reply
├───────────────────────────────┤
│  Ethernet II  — MAC frames    │  EtherType 0x0800
├───────────────────────────────┤
│  E1000 DMA    — NIC registers │  TX/RX descriptor rings
├───────────────────────────────┤
│  VMnet Host-Only              │  Virtual switch between VMs
└───────────────────────────────┘
```

## License

Built as a final semester project for Operating Systems and Computer Communication Networks.
**SOS — Shalu Operating System v1.0 — 2025**