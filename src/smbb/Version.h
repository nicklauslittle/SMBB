
/**
Copyright (c) 2019 Nick Little

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef SMBB_VERSION_H
#define SMBB_VERSION_H

#define SMBB_VERSION_QUOTE_(X) #X
#define SMBB_VERSION_QUOTE(X) SMBB_VERSION_QUOTE_(X)

#define SMBB_MAJOR_VERSION 0
#define SMBB_MINOR_VERSION 4
#define SMBB_BUGFIX_VERSION 0

#define SMBB_VERSION_STRING SMBB_VERSION_QUOTE(SMBB_MAJOR_VERSION) "." SMBB_VERSION_QUOTE(SMBB_MINOR_VERSION) "." SMBB_VERSION_QUOTE(SMBB_BUGFIX_VERSION)

#endif
