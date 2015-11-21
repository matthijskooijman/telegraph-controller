/*
 *
 *
 *    CwDecoderLogic.cs
 *
 *    Translates detected Morse elements into text.
 *
 *    License: GNU General Public License Version 3.0.
 *    
 *    Copyright (C) 2014 by Matthew K. Roberts, KK5JY. All rights reserved.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *    
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *    
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see: http://www.gnu.org/licenses/
 *    
 *
 */
 
#ifndef __CW_DECODER_LOGIC_H
#define __CW_DECODER_LOGIC_H

#include <WString.h>
#include <string.h>
#include <ctype.h>

namespace KK5JY {
	namespace CW {
		/// <summary>
		/// Decodes International Morse Code into a character stream.
		/// </summary>
		class CwDecoderLogic {
			private:
				typedef unsigned char byte;
			private:
				/// <summary>
				/// The lookup table: an array of { length, pattern, decoded_char }
				/// </summary>
				byte** m_Mapping;

				/// <summary>
				/// The hash table: an array of offsets into each table section
				/// </summary>
				byte* m_Hashes;

				/// <summary>
				/// The maximum number of elements per character.
				/// </summary>
				int m_MaxElements;
				
				/// <summary>
				/// The size of the mapping table.
				/// </summary>
				int m_MappingLength;

			public:
				/// <summary>
				/// The symbol to print if decoding fails for a single character.
				/// </summary>
				char ErrorSymbol;

				/// <summary>
				/// Returns the number of symbols in the lookup table.
				/// </summary>
				int SymbolCount() {
					return m_MappingLength;
				}

			private:
				/// <summary>
				/// Converts a string/char pair into a table entry.
				/// </summary>
				/// <param name="sPattern">The pattern of marks.</param>
				/// <param name="decodedValue">The character to decode when matching this pattern.</param>
				/// <returns>The table entry, a byte[3] array containing { length, pattern, decoded_char }.</returns>
				byte* MakeTableEntry(const char *sPattern, char decodedValue) {
					// create the pattern
					byte mask = 1;
					byte pattern = 0;
					byte len = strlen(sPattern);
					for (int i = 0; i != len; ++i) {
						if (sPattern[i] == '-')
							pattern |= mask;
						mask <<= 1;
					}

					// create the overall entry
					byte* result = new byte[3];
					result[0] = len;					// length
					result[1] = pattern;				// pattern
					result[2] = (byte)(decodedValue);	// value
					return result;
				}

				/// <summary>
				/// Search for a pattern.
				/// </summary>
				/// <param name="symbol">The symbol.</param>
				/// <param name="sLen">The symbol length.</param>
				/// <returns>The decoded character.</returns>
				char Lookup(byte symbol, byte sLen) {
					// if the length is invalid, just give up now
					if (sLen <= 0 || sLen > m_MaxElements) {
						return ErrorSymbol;
					}

					// lookup the hash, and start looking for the pattern there
					for (int i = m_Hashes[sLen - 1]; i != m_MappingLength; ++i) {
						// fetch the next table entry
						byte *entry = m_Mapping[i];

						// if the end of the table section has been reached, give up
						if (entry[0] != sLen)
							break;

						// if a match is found, return that
						if (entry[1] == symbol) {
							return (char)entry[2];
						}
					}

					// no luck?
					return ErrorSymbol;
				}

				/// <summary>
				/// Find a symbol encoding.
				/// </summary>
				bool Lookup(char ch, byte &pattern, byte &length) {
					// lookup the hash, and start looking for the pattern there
					for (int i = 0; i != m_MappingLength; ++i) {
						// fetch the next table entry
						byte *entry = m_Mapping[i];

						// if a match is found, return that
						if (entry[2] == ch) {
							length = entry[0];
							pattern = entry[1];
							return true;
						}
					}
					return false;
				}
				

			public:
				/// <summary>
				/// Initialize the decoder.
				/// </summary>
				CwDecoderLogic() {
					// the default error symbol
					ErrorSymbol = '~';

					// lookup table, sorted by pattern length, and roughly lexically
					m_MappingLength = 56;
					m_Mapping = new byte*[m_MappingLength];
					byte **bp = m_Mapping;
					char fbuf[10]; // used to translate from flash literals
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".")),    'E');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-")),    'T');

					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-")),   'A');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..")),   'I');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--")),   'M');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.")),   'N');

					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-..")),  'D');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--.")),  'G');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("---")),  'O');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.-")),  'K');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-.")),  'R');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("...")),  'S');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..-")),  'U');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".--")),  'W');
		
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-...")), 'B');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.-.")), 'C');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..-.")), 'F');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("....")), 'H');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".---")), 'J');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-..")), 'L');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".--.")), 'P');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--.-")), 'Q');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("...-")), 'V');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-..-")), 'X');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.--")), 'Y');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--..")), 'Z');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-.-")), '\n');

					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-----")), '0');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".----")), '1');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..---")), '2');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("...--")), '3');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("....-")), '4');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".....")), '5');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-....")), '6');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--...")), '7');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("---..")), '8');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("----.")), '9');
/*
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-..-.")), '/');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-...")), '&');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-...-")), '=');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-.-.")), '+');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.--.")), '(');
*/
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-.-.-")), '.');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("--..--")), ',');
				/*	*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..--..")), '?');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".----.")), ',');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.-.--")), ',');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.--.-")), ')');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("---...")), ':');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.-.-.")), ';');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-....-")), '-');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("..--.-")), '_');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".-..-.")), '"');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR(".--.-.")), '@');
					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("-.-.--")), '!');

					*bp++ = MakeTableEntry(strcpy_P(fbuf, PSTR("...-..-")), '$');
					*/
					
					// compute maximum element size
					m_MaxElements = 0;
					for (int i = 0; i != m_MappingLength; ++i) {
						byte len = m_Mapping[i][0];
						if (m_Mapping[i][0] > m_MaxElements) {
							m_MaxElements = len;
						}
					}
					// initialize hashes table
					m_Hashes = new byte[m_MaxElements];
					for (int i = 0; i != m_MaxElements; ++i) {
						m_Hashes[i] = 0;
					}
					// configure hashes
					for (int i = 0; i != m_MappingLength; ++i) {
						byte len = m_Mapping[i][0];
						if (len > 1 && m_Hashes[len - 1] == 0) {
							m_Hashes[len - 1] = (byte)i;
						}
					}
				}

				/// <summary>
				/// Do the decoding.
				/// </summary>
				/// <param name="raw">The raw element stream.</param>
				/// <returns>Decoded text.</returns>
				int Decode(CircularBuffer<MorseElements> &rxBuffer, char *buffer, int buflen) {
					int result = 0;
					byte symbol = 0;	// the symbol shift register
					byte mask = 1;		// the current bit mask
					byte bits = 0;		// the number of bits/marks (dot, dash) in the current symbol
					bool done = false;	// indicates that the current pattern should be consumed
					bool word = false;	// indicates that the current pattern is the last character in a word
					int count = 0;		// counts how many items to consume from RX buffer once character is decoded (includes space)
					do {
						// reset the counters and flags
						symbol = 0;
						mask = 1;
						bits = 0;
						done = false;
						word = false;
						count = 0;

						// run through the RX buffer
						for (count = 0; count != rxBuffer.Count() && !done; ++count) {
							// fetch the next item from the RX buffer
							MorseElements element = rxBuffer.ItemAt(count);

							// what kind of element is this?
							switch (element) {
								case WordSpace:
									word = true;
									done = true;
									break;
								case DashSpace:
									done = true;
									break;
								case Dot:
									// just leave the zero
									mask <<= 1;
									bits++;
									break;
								case Dash:
									bits++;
									symbol |= mask;
									mask <<= 1;
									break;
							}
							if (bits >= m_MaxElements) done = true;
						}

						// if an entire symbol was read...
						if (done) {
							// slice off the items we are consuming for this character
							rxBuffer.RemoveItems(count);

							// if the pattern wasn't just empty space
							if (bits) {
								// now look up the symbol
								char ch = Lookup(symbol, bits);
								if (ch != 0) {
									*buffer++ = ch;
									result++;
								}
							}

							// if this is the end of a word, add a space
							if (word) {
								*buffer++ = ' ';
								result++;
							}
						}
					} while (done);

					// return what we could decode
					return result;
				}
			
				/// <summary>
				/// Do the encoding.
				/// </summary>
				int Encode(CircularBuffer<MorseElements> &txBuffer, const char *buffer, int bufLen) {
					byte pattern;
					byte patLen;
					byte mask;
					char ch;
					int result = 0;
					bool marked = false;
					for (int i = 0; i != bufLen; ++i) {
						ch = buffer[i];
						if (isspace(ch)) {
							txBuffer.Add(WordSpace);
							++result;
							continue;
						}
						if (!Lookup(ch, pattern, patLen)) continue;
						mask = 1;
						for (int j = 0; j != patLen; ++j) {
							if ((pattern & mask) != 0) {
								// dash
								if (marked)
									txBuffer.Add(DotSpace);
								txBuffer.Add(Dash);
								marked = true;
							} else {
								// dot
								if (marked)
									txBuffer.Add(DotSpace);
								txBuffer.Add(Dot);
								marked = true;
							}
							mask <<= 1;
						}
						txBuffer.Add(DashSpace);
						++result;
					}
					return result;
				}
		};
	}
}

#endif
