#pragma ident "$Id: MJD.cpp 1649 2009-01-27 23:53:20Z raindave $"



//============================================================================
//
//  This file is part of GPSTk, the GPS Toolkit.
//
//  The GPSTk is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation; either version 2.1 of the License, or
//  any later version.
//
//  The GPSTk is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with GPSTk; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  
//  Copyright 2004, The University of Texas at Austin
//
//============================================================================

//============================================================================
//
//This software developed by Applied Research Laboratories at the University of
//Texas at Austin, under contract to an agency or agencies within the U.S. 
//Department of Defense. The U.S. Government retains all rights to use,
//duplicate, distribute, disclose, or release this software. 
//
//Pursuant to DoD Directive 523024 
//
// DISTRIBUTION STATEMENT A: This software has been approved for public 
//                           release, distribution is unlimited.
//
//=============================================================================

#include <cmath>
#include "MJD.hpp"
#include "TimeConstants.hpp"



namespace gpstk
{
   MJD& MJD::operator=( const MJD& right )
      throw()
   {
      mjd = right.mjd;
      timeSystem = right.timeSystem;
      return *this;
   }
   
   CommonTime MJD::convertToCommonTime() const
      throw( gpstk::InvalidRequest )
   {
      try
      {
            // convert to Julian Day
         long double tmp( mjd + MJD_JDAY );
            // get the whole number of days
         long jday( static_cast<long>( tmp ) );
            // tmp now holds the partial days
         tmp -= static_cast<long>( tmp );
            // convert tmp to seconds of day
         tmp *= SEC_PER_DAY;
      
         return CommonTime( jday,
                            static_cast<long>( tmp ),
                            tmp - static_cast<long>( tmp ),
                            timeSystem );
      }
      catch (InvalidParameter& ip)
      {
         InvalidRequest ir(ip);
         GPSTK_THROW(ip);
      }
      
   }
   
   void MJD::convertFromCommonTime( const CommonTime& ct )
      throw()
   {
      long jday, sod;
      double fsod;
      ct.get( jday, sod, fsod, timeSystem );
     
      mjd =  static_cast<long double>( jday - MJD_JDAY ) +
           (  static_cast<long double>( sod ) 
            + static_cast<long double>( fsod ) ) * DAY_PER_SEC;
   }
   
   std::string MJD::printf( const std::string& fmt ) const
      throw( gpstk::StringUtils::StringException )
   {
      try
      {
         using gpstk::StringUtils::formattedPrint;
         std::string rv( fmt );
         
         rv = formattedPrint( rv, getFormatPrefixFloat() + "Q",
                              "QLf", mjd );
         rv = formattedPrint( rv, getFormatPrefixInt() + "P",
                              "Ps", timeSystem.asString().c_str() );
         return rv;
      }
      catch( gpstk::StringUtils::StringException& se )
      {
         GPSTK_RETHROW( se );
      }
   }
   
   std::string MJD::printError( const std::string& fmt ) const
      throw( gpstk::StringUtils::StringException )
   {
      try
      {
         using gpstk::StringUtils::formattedPrint;
         std::string rv( fmt );
         
         rv = formattedPrint( rv, getFormatPrefixFloat() + "Q",
                              "Qs", getError().c_str() );
         rv = formattedPrint( rv, getFormatPrefixInt() + "P",
                              "Ps", getError().c_str() );
         return rv;         
      }
      catch( gpstk::StringUtils::StringException& se )
      {
         GPSTK_RETHROW( se );
      }
   }
   
   bool MJD::setFromInfo( const IdToValue& info )
      throw()
   {
      using namespace gpstk::StringUtils;
      
      for( IdToValue::const_iterator i = info.begin(); i != info.end(); i++ )
      {
         switch( i->first )
         {
            case 'Q':
               mjd = asLongDouble( i->second );
               break;

            case 'P':
               timeSystem = static_cast<TimeSys::Systems>(asInt( i->second ));
               break;

            default:
                  // do nothing
               break;
         };
      }      

      return true;
   }
   
   bool MJD::isValid() const
      throw()
   {
      MJD temp;
      temp.convertFromCommonTime( convertToCommonTime() );
      if( *this == temp )
      {
         return true;
      }
      return false;
   }
   
   void MJD::reset()
      throw()
   {
      mjd = 0.0;
      timeSystem = TimeSys::Unknown;
   }

   bool MJD::operator==( const MJD& right ) const
      throw()
   {
     /// Any (wildcard) type exception allowed, otherwise must be same time systems
      if ((timeSystem.getTimeSystem() != TimeSys::Any &&
           right.timeSystem.getTimeSystem() != TimeSys::Any) &&
          timeSystem != right.timeSystem)
         return false;

      if( fabs(mjd - right.mjd) < CommonTime::eps )
      {
         return true;
      }
      return false;
   }

   bool MJD::operator!=( const MJD& right ) const
      throw()
   {
      return ( !operator==( right ) );
   }

   bool MJD::operator<( const MJD& right ) const
      throw( gpstk::InvalidRequest )
   {
     /// Any (wildcard) type exception allowed, otherwise must be same time systems
      if ((timeSystem.getTimeSystem() != TimeSys::Any &&
           right.timeSystem.getTimeSystem() != TimeSys::Any) &&
          timeSystem != right.timeSystem)
      {
         gpstk::InvalidRequest ir("CommonTime objects not in same time system, cannot be compared");
         GPSTK_THROW(ir);
      }

      if( mjd < right.mjd )
      {
         return true;
      }
      return false;
   }

   bool MJD::operator>( const MJD& right ) const
      throw( gpstk::InvalidRequest )
   {
      return ( !operator<=( right ) );
   }

   bool MJD::operator<=( const MJD& right ) const
      throw( gpstk::InvalidRequest )
   {
      return ( operator<(  right ) ||
               operator==( right )   );
   }

   bool MJD::operator>=( const MJD& right ) const
      throw( gpstk::InvalidRequest )
   {
      return ( !operator<( right ) );
   }

} // namespace