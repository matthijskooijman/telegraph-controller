/*
 *
 *
 *    Elements.h
 *
 *    CW element encoding.
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
 
 #ifndef __ELEMENTS_H
#define __ELEMENTS_H

namespace KK5JY {
	namespace CW {
		/// <summary>
		/// A detected CW element.
		/// </summary>
		struct CwElement {
			/// <summary>
			/// The length of the pulse or space.
			/// </summary>
			unsigned Length;

			/// <summary>
			/// Indicates that this is a key-down element (Mark).  Otherwise,
			/// this element is a space.
			/// </summary>
			bool Mark;
			
			//
			//  Default ctor
			//
			CwElement() {
				Length = 0.0;
				Mark = false;
			}
			
			//
			//  Copy ctor
			//
			CwElement(const CwElement &e) {
				Length = e.Length;
				Mark = e.Mark;
			}
			
			//
			//  operator=
			//
			CwElement &operator=(const CwElement &e) {
				Length = e.Length;
				Mark = e.Mark;
				return *this;
			}
		};
		
		/// <summary>
		/// Element types.
		/// </summary>
		enum MorseElements {
			/// <summary>
			/// A dot-equivalent space.
			/// </summary>
			DotSpace,

			/// <summary>
			/// A dash-equivalent space.
			/// </summary>
			DashSpace,

			/// <summary>
			/// A word space.
			/// </summary>
			WordSpace,

			/// <summary>
			/// A dot.
			/// </summary>
			Dot,

			/// <summary>
			/// A dash.
			/// </summary>
			Dash
		};
	}
}
#endif
