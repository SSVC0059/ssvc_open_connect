##
## UART ANSI — Sigrok protocol decoder (libsigrokdecode API v3).
## Parses ECMA-48 / VT100-style ANSI escape sequences on top of the UART decoder.
##
## Install: copy the whole `uart_ansi` folder (pd.py + __init__.py) into the
## decoders directory next to `modbus`, `1-uart`, etc.
## DSView: `C:\Program Files\DSView\decoders\uart_ansi\`
## Stack: Logic → **1:UART** (exports `uart`) → UART ANSI. Do not use 0:UART
## as the bottom layer — it does not publish the `uart` stream for stacking.
##
## Debug: set env SIGROK_UART_ANSI_DEBUG=1 and watch stderr (DSView log / console).
## The UART decoder feeds stacked PDs with DATA events (like Modbus), not FRAME.
##
## SPDX-License-Identifier: GPL-2.0-or-later
##

import os
import sys

import sigrokdecode as srd


def _debug(msg):
    if os.environ.get('SIGROK_UART_ANSI_DEBUG', ''):
        print('[uart_ansi] {}'.format(msg), file=sys.stderr, flush=True)

RX = 0
TX = 1

# CSI final byte (0x40–0x7E) → short description
_CSI_FINAL = {
    '@': 'ICH (insert char)',
    'A': 'CUU (cursor up)',
    'B': 'CUD (cursor down)',
    'C': 'CUF (cursor forward)',
    'D': 'CUB (cursor back)',
    'E': 'CNL (cursor next line)',
    'F': 'CPL (cursor previous line)',
    'G': 'CHA (cursor col)',
    'H': 'CUP (cursor position)',
    'f': 'CUP (cursor position)',
    'J': 'ED (erase in display)',
    'K': 'EL (erase in line)',
    'S': 'SU (scroll up)',
    'T': 'SD (scroll down)',
    'm': 'SGR (select graphic rendition)',
    's': 'SCP (save cursor)',
    'u': 'RCP (restore cursor)',
    'h': 'SM (set mode)',
    'l': 'RM (reset mode)',
    'n': 'DSR (device status)',
    'c': 'DA (device attributes)',
    'g': 'TBC (tab clear)',
    'r': 'DECSTBM (set margins)',
    'L': 'IL (insert line)',
    'M': 'DL (delete line)',
    'P': 'DCH (delete char)',
    'X': 'ECH (erase char)',
    '~': 'special (keyboard / bracketed paste)',
}

# ESC + letter (Fe sequences, 0x40–0x5F)
_ESC_FE = {
    ord('D'): 'IND (index)',
    ord('E'): 'NEL (next line)',
    ord('H'): 'HTS (tab set)',
    ord('M'): 'RI (reverse index)',
    ord('Z'): 'DECID',
    ord('c'): 'RIS (reset)',
    ord('N'): 'SS2 (single shift 2)',
    ord('O'): 'SS3 (single shift 3)',
}


def _describe_csi(body):
    """body is the CSI payload after ESC, starting with '[' through final byte."""
    if not body.startswith('['):
        return 'CSI'
    inner = body[1:]
    if not inner:
        return 'CSI (empty)'
    final = inner[-1]
    hint = _CSI_FINAL.get(final, 'CSI')
    if inner.startswith('?'):
        return 'CSI private {} ({})'.format(inner, hint)
    return '{} [{}]'.format(hint, inner)


def _ctrl_name(b):
    _names = {
        0x00: 'NUL', 0x07: 'BEL', 0x08: 'BS', 0x09: 'TAB',
        0x0A: 'LF', 0x0B: 'VT', 0x0C: 'FF', 0x0D: 'CR',
        0x0E: 'SO', 0x0F: 'SI', 0x18: 'CAN', 0x1A: 'SUB', 0x1B: 'ESC',
    }
    return _names.get(b, 'C0 0x{:02X}'.format(b))


class AnsiStream:
    """Per-direction ANSI parser."""

    def __init__(self, dec, rxtx):
        self.dec = dec
        self.rxtx = rxtx
        self.reset()

    def reset(self):
        self.state = 'normal'
        self.text_ss = None
        self.text_es = None
        self.text_bytes = bytearray()
        self.seq_ss = None
        self.csi_buf = bytearray()
        self.osc_buf = bytearray()
        self.charset_byte = None

    def put_ann(self, ss, es, key, msg):
        idx = self.dec.ann_index[key]
        self.dec.put(ss, es, self.dec.out_ann, [idx, [msg]])

    def flush_text(self):
        if self.text_ss is None:
            return
        raw = bytes(self.text_bytes)
        try:
            s = raw.decode('utf-8')
        except UnicodeDecodeError:
            s = raw.decode('latin-1', errors='replace')
        self.put_ann(self.text_ss, self.text_es, 'text', s)
        self.text_ss = None
        self.text_es = None
        self.text_bytes = bytearray()

    def append_text(self, ss, es, b):
        if self.text_ss is None:
            self.text_ss = ss
        self.text_es = es
        self.text_bytes.append(b)

    def feed(self, ss, es, b):
        b &= 0xFF

        if self.state == 'normal':
            if b == 0x1B:
                self.flush_text()
                self.state = 'esc'
                self.seq_ss = ss
                return
            if b == 0x0A:
                self.append_text(ss, es, b)
                self.flush_text()
                return
            if b in (0x09, 0x0D):
                self.append_text(ss, es, b)
                return
            if b < 0x20 or b == 0x7F:
                self.flush_text()
                self.put_ann(ss, es, 'ctrl', _ctrl_name(b))
                return
            self.append_text(ss, es, b)
            return

        if self.state == 'esc':
            self._feed_esc(ss, es, b)
            return
        if self.state == 'csi':
            self._feed_csi(ss, es, b)
            return
        if self.state == 'osc':
            self._feed_osc(ss, es, b)
            return
        if self.state == 'osc_esc':
            self._feed_osc_esc(ss, es, b)
            return
        if self.state == 'ss3':
            self._finish_esc_seq(self.seq_ss, es, 'SS3 0x{:02X}'.format(b))
            return
        if self.state == 'charset':
            self._finish_esc_seq(self.seq_ss, es,
                'Charset select 0x{:02X} 0x{:02X}'.format(self.charset_byte, b))
            self.charset_byte = None
            return

    def _finish_esc_seq(self, ss, es, msg):
        self.put_ann(ss, es, 'esc', msg)
        self.state = 'normal'

    def _feed_esc(self, ss, es, b):
        if b == 0x1B:
            self._finish_esc_seq(self.seq_ss, es, 'ESC ESC')
            return
        if b == ord('['):
            self.state = 'csi'
            self.csi_buf = bytearray()
            return
        if b == ord(']'):
            self.state = 'osc'
            self.osc_buf = bytearray()
            return
        if b == ord('O'):
            self.state = 'ss3'
            return
        if b in (0x37, 0x38):
            name = 'DECSC (save cursor)' if b == 0x37 else 'DECRC (restore cursor)'
            self._finish_esc_seq(self.seq_ss, es, 'ESC {}'.format(name))
            return
        if b in (0x28, 0x29, 0x2D):
            self.state = 'charset'
            self.charset_byte = b
            return
        if 0x40 <= b <= 0x5F:
            name = _ESC_FE.get(b, 'Fe 0x{:02X}'.format(b))
            self._finish_esc_seq(self.seq_ss, es, 'ESC {}'.format(name))
            return
        self._finish_esc_seq(self.seq_ss, es, 'ESC 0x{:02X}'.format(b))

    def _feed_csi(self, ss, es, b):
        self.csi_buf.append(b)
        if 0x40 <= b <= 0x7E:
            raw = '[' + ''.join(chr(x) for x in self.csi_buf)
            msg = _describe_csi(raw)
            self.put_ann(self.seq_ss, es, 'csi', msg)
            self.state = 'normal'
            self.csi_buf = bytearray()

    def _feed_osc(self, ss, es, b):
        if b == 0x07:
            self._finish_osc(es)
            return
        if b == 0x1B:
            self.state = 'osc_esc'
            return
        self.osc_buf.append(b)

    def _feed_osc_esc(self, ss, es, b):
        if b == 0x5C:
            self._finish_osc(es)
            return
        self.osc_buf.append(0x1B)
        self.osc_buf.append(b)
        self.state = 'osc'

    def _finish_osc(self, es):
        try:
            s = self.osc_buf.decode('utf-8', errors='replace')
        except Exception:
            s = str(self.osc_buf)
        self.put_ann(self.seq_ss, es, 'osc', 'OSC {}'.format(s))
        self.osc_buf = bytearray()
        self.state = 'normal'


class Decoder(srd.Decoder):
    api_version = 3
    id = 'uart_ansi'
    name = 'UART ANSI'
    longname = 'UART ANSI / VT100 escape sequences'
    desc = 'ANSI/ECMA-48 escape sequences on asynchronous serial (stack on UART).'
    license = 'gplv2+'
    inputs = ['uart']
    outputs = []
    tags = ['Serial', 'Terminal']

    annotations = (
        ('text', 'Text / UTF-8'),
        ('ctrl', 'C0 control'),
        ('esc', 'Escape sequence'),
        ('csi', 'CSI sequence'),
        ('osc', 'OSC sequence'),
    )
    annotation_rows = (
        ('stream', 'Decoded stream', (0, 1, 2, 3, 4)),
    )

    options = (
        {'id': 'rxtx', 'desc': 'UART direction', 'default': 'both',
         'values': ('rx', 'tx', 'both'), 'idn': 'dec_uart_ansi_opt_rxtx'},
        {'id': 'byte_event', 'desc': 'UART event (DATA matches DSView Modbus)',
         'default': 'data',
         'values': ('data', 'frame', 'auto'), 'idn': 'dec_uart_ansi_opt_byte_event'},
    )

    def __init__(self):
        self.out_ann = None
        self.ann_index = {}
        self.reset()

    def reset(self):
        self.rx = AnsiStream(self, RX)
        self.tx = AnsiStream(self, TX)
        self._had_data = False
        self._debug_count = 0

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)
        self.ann_index = {a[0]: i for i, a in enumerate(self.annotations)}

    def _want(self, rxtx):
        c = self.options['rxtx']
        if c == 'both':
            return True
        if c == 'rx':
            return rxtx == RX
        if c == 'tx':
            return rxtx == TX
        return True

    def decode(self, ss, es, data):
        ptype, rxtx, pdata = data
        mode = self.options['byte_event']

        b = None
        if mode == 'data':
            if ptype != 'DATA':
                return
            b = int(pdata[0]) & 0xFF
        elif mode == 'frame':
            if ptype != 'FRAME':
                return
            val, valid = pdata
            if not valid:
                return
            b = int(val) & 0xFF
        else:
            # auto: UART emits DATA then FRAME per byte — use DATA only if present
            if ptype == 'DATA':
                self._had_data = True
                b = int(pdata[0]) & 0xFF
            elif ptype == 'FRAME':
                if self._had_data:
                    return
                val, valid = pdata
                if not valid:
                    return
                b = int(val) & 0xFF
            else:
                return

        if os.environ.get('SIGROK_UART_ANSI_DEBUG', '') and self._debug_count < 8:
            _debug('ptype={} rxtx={} b=0x{:02X}'.format(ptype, rxtx, b))
            self._debug_count += 1

        if not self._want(rxtx):
            return
        if rxtx == RX:
            self.rx.feed(ss, es, b)
        else:
            self.tx.feed(ss, es, b)

    def end(self):
        """Flush pending text at end of capture (if the runtime calls it)."""
        self.rx.flush_text()
        self.tx.flush_text()
