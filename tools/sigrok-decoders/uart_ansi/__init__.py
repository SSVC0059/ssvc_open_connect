##
## UART ANSI — Sigrok protocol decoder package (DSView / PulseView).
##
## SPDX-License-Identifier: GPL-2.0-or-later
##

'''
This decoder stacks on the UART protocol decoder (`inputs = ['uart']`) and
annotates ECMA-48 / VT100-style ANSI escape sequences.

DSView: use the **1:UART** decoder (not 0:UART) so the stack exports the
`uart` protocol for child decoders. Order: Logic → 1:UART → UART ANSI.
'''

from .pd import Decoder
