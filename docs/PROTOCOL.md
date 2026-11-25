# Panel protocol notes

The S1 front display is a 320x170 RGB565 panel driven by a Holtek HT32 microcontroller
that appears as USB device `04d9:fd01`. Interface 1 has one interrupt OUT endpoint
(`0x02`, 64-byte packets, full speed). Its HID descriptor is malformed, so the kernel's
hidraw interface does not work and this program talks to it with libusb.

The command set was documented by
[tjaworski/AceMagic-S1-LED-TFT-Linux](https://github.com/tjaworski/AceMagic-S1-LED-TFT-Linux).
The timing behaviour below was measured for this project on one S1.

## Commands

Every command is a single 4104-byte transfer: an 8-byte header followed by 4096 bytes
of payload (zero padded).

| Bytes | Command | Meaning |
| --- | --- | --- |
| `55 A1 F1 oo` | orientation | `01` landscape, `02` portrait |
| `55 A1 F2 hh mm ss` | heartbeat | keepalive, carries the time of day |
| `55 A1 F3 hh mm ss` | set time | sets the firmware clock |
| `55 A2 xl xh yl yh w h` + pixels | update | draw a `w` x `h` rectangle at `x`,`y` |
| `55 A3 F0/F1/F2 seq ...` + pixels | redraw | whole screen in 27 chunks (**not used**) |

Pixels are RGB565 with the high byte first, row by row. An update carries at most
4096 bytes, so `w * h <= 2048`, and `w <= 254` because it travels in one byte.

Without a heartbeat about once a second the firmware replaces the picture with a
"disconnection" notice.

## Timing

| Operation | Measured |
| --- | --- |
| One command (any kind) | 65 ms; the endpoint moves 64 bytes per millisecond |
| Whole screen as 30 update tiles | about 2.0 s, no failures, heartbeats interleaved |
| Whole screen with the redraw command | about 2.4 s |
| After a redraw completes | no command is accepted for 6 to 7 s |

The last row is why this program never sends a redraw. A program that redraws the
whole screen in a loop gets one picture through roughly every 9 seconds and sees
every transfer in between time out. Updates have no such penalty, so the display is
driven entirely with them:

1. Keep a copy of what the panel currently shows.
2. Render the next frame and compare.
3. Cover the differences with rectangles that fit one update each, top to bottom.
4. Send them; a rectangle that times out stays marked as changed and is retried.

A clock tick costs one or two commands; changing page repaints the screen top to
bottom in about two seconds, which is the limit of the link.

## LED strip

The LED strip behind the front panel is a separate device (CH340 serial adapter,
`1a86:7523`). This program does not control it.
