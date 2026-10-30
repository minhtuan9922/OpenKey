# WHATWG named character reference data

The table in HtmlNamedEntities.h is derived from the WHATWG HTML Standard:
https://html.spec.whatwg.org/entities.json
https://html.spec.whatwg.org/multipage/named-characters.html

Retrieved 2026-10-10. Only the 2,125 semicolon-terminated names are retained
(93 map to two Unicode scalars). Keys omit the leading ampersand and trailing
semicolon, are sorted ordinally, and code points are rendered as C++ hexadecimal
integers. A separate sorted list now retains the 106 historical no-semicolon
aliases from the same data, retrieved 2026-10-10. The table is compiled into the
executable; no runtime download occurs.

Copyright © WHATWG (Apple, Google, Mozilla, Microsoft).

The HTML Standard's source-code incorporation terms specify BSD 3-Clause:
https://html.spec.whatwg.org/multipage/acknowledgements.html#ipr

The numeric C1 replacement mapping in HtmlClipboard.h is also derived from:
https://html.spec.whatwg.org/multipage/parsing.html#numeric-character-reference-end-state
It is stored as a dense 32-entry table; the five unmapped C1 controls map to
themselves. The source-code incorporation notice below also covers this table.

## BSD 3-Clause License — incorporated table

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This notice covers the incorporated table, not all dependencies of the beta.
The project's broader release/license review remains pending.
