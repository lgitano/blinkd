/* File: blinkd.h
   (C) 1998 W. Martin Borgert debacle@debian.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301, USA.
*/

#define RATE_DEC 0x1F           /* '00111111'B */
#define RATE_INC (RATE_DEC - 1) /* '00111110'B */

typedef enum {BLINKD_CAP, BLINKD_NUM, BLINKD_SCR, BLINKD_ALL} leds_t;
