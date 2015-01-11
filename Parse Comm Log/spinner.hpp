#pragma once

#include <iostream>  // for std::cout

using std::cout;

// Update a spinner display given a simple counter
inline void updateSpinner(unsigned int spin_count)
{
   switch (spin_count % 4)
   {
      case 0:
      {
         cout << "/\r";
      }
      break;

      case 1:
      {
         cout << "-\r";
      }
      break;

      case 2:
      {
         cout << "\\\r";
      }
      break;

      case 3:
      {
         cout << "|\r";
      }
      break;

      default:
         break;
   }
}
