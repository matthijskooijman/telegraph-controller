/*
 *
 *
 *    CwTimingLogic.h
 *
 *    Translates detected tone pulses into morse elements.
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
 
#ifndef __CW_TIMING_H
#define __CW_TIMING_H

#include "CircularBuffer.h"
#include "Elements.h"
#include <float.h>

using namespace KK5JY::Collections;

namespace KK5JY {
	namespace CW {
		// speed control states
		enum SpeedSources {
			SpeedManual = 0,
			SpeedAuto = 1
		};

		/// <summary>
		/// Translates detected elements into a logical symbol stream.
		/// </summary>
		class CwTimingLogic {
			private:
				/// <summary>
				/// The current average dot length.
				/// </summary>
				float m_DotLength;

				/// <summary>
				/// The current average dot length used for TX.
				/// </summary>
				float m_TxDotLength;

				/// <summary>
				/// The current average dot length used for RX.
				/// </summary>
				float m_RxDotLength;

				/// <summary>
				/// The boxcar array of previous mark lengths.
				/// </summary>
				float *m_BoxCar;

				/// <summary>
				/// The current boxcar sum.
				/// </summary>
				float m_BoxCarSum;
				
				/// <summary>
				/// The current boxcar average.
				/// </summary>
				float m_BoxCarAverage;
				
				/// <summary>
				/// The minimum average distance as a percentage.
				/// </summary>
				float m_MinimumAverageDistance;

				/// <summary>
				/// The index of the next item to write into the boxcar.
				/// </summary>
				unsigned m_BoxCarIndex;
				
				/// <summary>
				/// The size of the boxcar array.
				/// </summary>
				unsigned m_BoxCarSize;
				
				/// <summary>
				/// The current gap, computed as per MinimumAverageDistance.  This
				/// is the value actually used on a per-element basis.  The value
				/// of MinimumAverageDistance is the user interface only.
				/// </summary>
				int m_SafetyGap;
				
				/// <summary>
				/// Sets the TX speed source.
				/// </summary>
				SpeedSources m_TxSpeedSource;

				/// <summary>
				/// Sets the RX speed source.
				/// </summary>
				SpeedSources m_RxSpeedSource;

			public:
				/// <summary>
				/// The maximum length for a dot or dot-space, as a multiple of the current average dot length.
				/// </summary>
				float MaximumDotLength;

				/// <summary>
				/// The minimum length for a word space, as a multiple of the current average dot length.
				/// </summary>
				float MinimumWordSpace;
				
				/// <summary>
				/// The minimum mark length to include in the moving average for timing track.
				/// </summary>
				float MinimumMark;

				/// <summary>
				/// The maximum mark length to include in the moving average for timing track.
				/// </summary>
				float MaximumMark;
				
				/// <summary>
				/// The distance, as a fraction of the average, that a new sample's pulse width must
				/// be in order to be counted in the average.  This prevents the average from collapsing
				/// in on itself when a long string of dots or dashes is encountered.
				/// </summary>
				float MinimumAverageDistance() const { return m_MinimumAverageDistance; }
				
				/// <summary>
				/// Set the new minimum average distance (see above).
				/// </summary>
				void MinimumAverageDistance(float mad) {
					m_MinimumAverageDistance = mad;
					InitializeBoxCar(m_DotLength);
				}

			public:
				/// <summary>
				/// Construct a new timing object.
				/// </summary>
				CwTimingLogic(float dotLength = 1.0, int bcLength = 8) : m_BoxCar(0) {
					// set some reasonable default timing limits
					MaximumDotLength = 2;
					MinimumWordSpace = 4.5;
					MinimumMark = 0.0;
					MaximumMark = FLT_MAX;
					
					// set the default minimum average distance
					m_MinimumAverageDistance = 0.35;  // 35%

					// initialize the boxcar
					AllocateBoxCar(bcLength);
					InitializeBoxCar(dotLength);
					
					// set speed sources
					m_TxSpeedSource = SpeedAuto;
					m_RxSpeedSource = SpeedAuto;
				}

			public: // properties
				/// <summary>
				/// Return the current boxcar length.
				/// </summary>
				unsigned BoxCarLength () const { return m_BoxCarSize; }

				/// <summary>
				/// Set a new boxcar length.
				/// </summary>
				void BoxCarLength (unsigned bcLength) {
					int dl = m_DotLength;
					AllocateBoxCar(bcLength);
					InitializeBoxCar(dl);
				}
				
				/// <summary>
				/// Return the current average dot length from the tracker.
				/// </summary>
				float DotLength() const { return m_DotLength; }
				
				/// <summary>
				/// Estimate the current RX WPM based on the average dot length.
				/// </summary>
				float RxWPM() const { return 1200 / m_RxDotLength; }
				
				/// <summary>
				/// Estimate the current RX WPM based on the average dot length.
				/// </summary>
				float TxWPM() const { return 1200 / m_TxDotLength; }
				
				/// <summary>
				/// Set the RX WPM, and drop into manual RX mode.
				/// </summary>
				void RxWPM(int wpm) {
					int newLength = 1200 / wpm;
					m_RxSpeedSource = SpeedManual;
					m_RxDotLength = newLength;
					InitializeBoxCar(newLength * 2); // average should be double the dot length
				}
				
				/// <summary>
				/// Set the TX WPM, and drop into manual TX mode.
				/// </summary>
				void TxWPM(int wpm) {
					int newLength = 1200 / wpm;
					m_TxSpeedSource = SpeedManual;
					m_TxDotLength = newLength;
				}

				//
				//  Get the RX speed source.
				//
				SpeedSources RxMode() const {
					return m_RxSpeedSource;
				}

				//
				//  Get the TX speed source.
				//
				SpeedSources TxMode() const {
					return m_TxSpeedSource;
				}
				
				//
				//  Set the RX speed source.
				//
				void RxMode(SpeedSources src) {
					m_RxSpeedSource = src;
				}

				//
				//  Set the TX speed source.
				//
				void TxMode(SpeedSources src) {
					m_TxSpeedSource = src;
				}
				
			public: // methods
				/// <summary>
				/// Do the decoding.
				/// </summary>
				/// <param name="raw">The raw element data.</param>
				/// <param name="result">A decoded element symbol stream.</param>
				/// <returns>True if a word space was added to the result, indicating data ready to decode.</returns>
				bool Decode(CircularBuffer<CwElement> &raw, CircularBuffer<MorseElements> &result) {
					bool space = false;
					while (raw.Count() != 0 && !result.Full()) {
						CwElement element;
						raw.Remove(element);
						// update the element length average
						if (element.Mark && (element.Length > MinimumMark) && (element.Length < MaximumMark)) {
							if (element.Length < (m_BoxCarAverage - m_SafetyGap) || element.Length > (m_BoxCarAverage + m_SafetyGap)) {
								m_BoxCarSum -= m_BoxCar[m_BoxCarIndex];
								m_BoxCarSum += element.Length;
								m_BoxCar[m_BoxCarIndex] = element.Length;
								m_BoxCarIndex = (m_BoxCarIndex + 1) % m_BoxCarSize;

								//
								//  Compute average dot length...
								//
								//  Since this is an average of all of the elements, the
								//  overall average should be close to the midpoint
								//  between dot and dash lengths.  One half of that should
								//  be roughly the dot length.
								//
								m_BoxCarAverage = m_BoxCarSum / m_BoxCarSize;
								m_DotLength = m_BoxCarAverage / 2;
								m_SafetyGap = m_MinimumAverageDistance * m_BoxCarAverage;
							}
						}
						if (m_RxSpeedSource == SpeedAuto) {
							m_RxDotLength = m_DotLength;
						}
						if (m_TxSpeedSource == SpeedAuto) {
							m_TxDotLength = m_DotLength;
						}

						// now decode the specific element type
						if (element.Length <= (MaximumDotLength * m_RxDotLength)) {
							// short elements
							result.Add(element.Mark ? Dot : DotSpace);
						} else if (element.Length >= (MinimumWordSpace * m_RxDotLength) && !element.Mark) {
							// word space
							result.Add(WordSpace);
							space = true;
						} else {
							// long elements
							result.Add(element.Mark ? Dash : DashSpace);
							if (!element.Mark)
								space = true;
						}
					}

					return space;
				}
				
				/// <summary>
				/// Do the decoding.
				/// </summary>
				int Encode(CircularBuffer<MorseElements> &txBuffer, CircularBuffer<CwElement> &cwBuffer) {
					MorseElements el;
					const int dotSpace = m_TxDotLength;
					const int dashSpace = dotSpace * 3;
					const int wordSpace = dotSpace * 5; //7; // word-space less dot-space on either end
					CwElement cw;
					int result = 0;
					
					while (txBuffer.Remove(el)) {
						switch (el) {
							case Dot:
								cw.Mark = true;
								cw.Length = dotSpace;
								cwBuffer.Add(cw);
								++result;
								break;
							case Dash:
								cw.Mark = true;
								cw.Length = dashSpace;
								cwBuffer.Add(cw);
								++result;
								break;
							case DotSpace:
								cw.Mark = false;
								cw.Length = dotSpace;
								cwBuffer.Add(cw);
								++result;
								break;
							case DashSpace:
								cw.Mark = false;
								cw.Length = dashSpace;
								cwBuffer.Add(cw);
								++result;
								break;
							case WordSpace:
								cw.Mark = false;
								cw.Length = wordSpace;
								cwBuffer.Add(cw);
								++result;
								break;
							default:
								// nop
								break;
						}
					}
					return result;
				}

			private:
				/// <summary>
				/// Allocate the boxcar space.
				/// </summary>
				void AllocateBoxCar(int bcLength) {
					m_BoxCarSize = bcLength;
					if (m_BoxCar != 0) {
						delete [] m_BoxCar;
					}
					m_BoxCar = new float[m_BoxCarSize];
				}
				
				/// <summary>
				/// Initialize the boxcar with a specific dot length
				/// </summary>
				void InitializeBoxCar(int dotLength) {
					m_BoxCarSum = 0;
					for (int i = 0; i != m_BoxCarSize; ++i) {
						m_BoxCar[i] = dotLength;
						m_BoxCarSum += dotLength;
					}
					m_BoxCarAverage = m_BoxCarSum / m_BoxCarSize;
					m_DotLength = m_BoxCarAverage / 2;
					m_SafetyGap = m_MinimumAverageDistance * m_BoxCarAverage;
					m_BoxCarIndex = 0;
				}
		};
	}
}
#endif
