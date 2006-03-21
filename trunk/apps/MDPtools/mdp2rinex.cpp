#pragma ident "$Id: //depot/sgl/gpstk/dev/apps/MDPtools/mdp2rinex.cpp#8 $"

/** @file Converts an MDP stream into RINEX obs/nav files */

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

#include "StringUtils.hpp"
#include "LoopedFramework.hpp"

#include "RinexObsStream.hpp"
#include "RinexObsData.hpp"
#include "RinexNavStream.hpp"
#include "RinexNavData.hpp"

#include "MDPStream.hpp"
#include "MDPNavSubframe.hpp"
#include "MDPObsEpoch.hpp"

#include "FormatConversionFunctions.hpp"

using namespace std;
using namespace gpstk;


class MDP2Rinex : public gpstk::LoopedFramework
{
public:
   MDP2Rinex(const std::string& applName)
      throw()
      : LoopedFramework(applName, "Converts an MDP stream to RINEX."),
        navFileOpt('n', "nav",   "Filename to write RINEX nav data to."),
        obsFileOpt('o', "obs",   "Filename to write RINEX obs data to. The filename of '-' means to use stdout.", true),
        mdpFileOpt('i', "mdp-input", "Filename to read MDP data from. The filename of '-' means to use stdin.", true),
        c2Opt('c', "l2c", "Enable output of L2C data in C2"),
        thinningOpt('t', "thinning", "A thinning factor for the data, specified in seconds between points. Default: none.")
   {
      navFileOpt.setMaxCount(1);
      obsFileOpt.setMaxCount(1);
      mdpFileOpt.setMaxCount(1);
   }

   bool initialize(int argc, char *argv[]) throw()
   {
      if (!LoopedFramework::initialize(argc,argv)) return false;

      if (mdpFileOpt.getCount())
         if (mdpFileOpt.getValue()[0] != "-")
            mdpInput.open(mdpFileOpt.getValue()[0].c_str());
         else
         {
            
            if (debugLevel)
               cout << "Taking input from stdin." << endl;
            mdpInput.copyfmt(std::cin);
            mdpInput.clear(std::cin.rdstate());
            mdpInput.std::basic_ios<char>::rdbuf(std::cin.rdbuf());
         }
      

      if (obsFileOpt.getCount())
         if (obsFileOpt.getValue()[0] != "-")
            rinexObsOutput.open(obsFileOpt.getValue()[0].c_str(), std::ios::out);
         else
         {
            if (debugLevel)
               cout << "Sending output to stdout." << endl;
            rinexObsOutput.copyfmt(std::cout);
            rinexObsOutput.clear(std::cout.rdstate());
            rinexObsOutput.std::basic_ios<char>::rdbuf(std::cout.rdbuf());
         }

      if (navFileOpt.getCount())
         rinexNavOutput.open(navFileOpt.getValue()[0].c_str(), std::ios::out);
      else
         rinexNavOutput.clear(std::ios::badbit);

      if (thinningOpt.getCount())
      {
         thin = true;
         thinning = gpstk::StringUtils::asInt(thinningOpt.getValue()[0]);
         if (debugLevel)
            cout << "Thinning data modulo " << thinning << " seconds." << endl;
      }
      else
         thin = false;

      firstObs = true;
      return true;
   }
   
protected:
   virtual void spinUp()
   {
      if (!mdpInput)
      {
         cout << "Error: could not open input." << endl;
         exit(-1);
      }

      mdpInput.exceptions(fstream::failbit);
      rinexObsOutput.exceptions(fstream::failbit);

      if (rinexNavOutput)
         rinexNavOutput.exceptions(fstream::failbit);
      
      roh.valid |= gpstk::RinexObsHeader::allValid21;
      roh.fileType = "Observation";
      roh.fileProgram = "mdp2rinex";
      roh.markerName = "Unknown";
      roh.observer = "Unknown";
      roh.agency = "Unknown";
      roh.antennaOffset = gpstk::Triple(0,0,0);
      roh.antennaPosition = gpstk::Triple(0,0,0);
      roh.wavelengthFactor[0] = 1;
      roh.wavelengthFactor[1] = 1;
      roh.recType = "Unknown MDP";
      roh.recVers = "Unknown";
      roh.recNo = "1";
      roh.antType = "Unknown";
      roh.antNo = "1";
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::C1);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::P1);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::L1);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::D1);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::P2);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::L2);
      roh.obsTypeList.push_back(gpstk::RinexObsHeader::D2);
      if (c2Opt.getCount())
         roh.obsTypeList.push_back(gpstk::RinexObsHeader::C2);

      rnh.valid = gpstk::RinexNavHeader::allValid21;
      rnh.fileType = "Navigation";
      rnh.fileProgram = "mdp2rinex";
      rnh.fileAgency = "Unknown";
      rnh.version = 2.1;

      if (rinexNavOutput)
         rinexNavOutput << rnh;
   }

   virtual void process(MDPNavSubframe& nav)
   {
      short sfid = nav.getSFID();
      if (sfid > 3)
         return;

      short week = nav.time.GPSfullweek();
      long sow = nav.getHOWTime();
      if ( sow >604800)
         return;

      DayTime howTime(week, sow);

      if (nav.range != rcCA || nav.carrier != ccL1)
         return;

      NavIndex ni(RangeCarrierPair(nav.range, nav.carrier), nav.prn);
      ephData[ni] = nav;

      long sfa[10];
      nav.fillArray(sfa);
      long long_sfa[10];

      for( int j = 0; j < 10; j++ )
         long_sfa[j] = static_cast<long>( sfa[j] );

      if (gpstk::EngNav::subframeParity(long_sfa))
      {
         ephPageStore[ni][sfid] = nav;
         EngEphemeris engEph;
         if (makeEngEphemeris(engEph, ephPageStore[ni]))
         {
            RinexNavData rnd(engEph);
            rinexNavOutput << rnd;
            ephPageStore[ni].clear();
         }
      }
   } // end of process(MDPNavSubframe)


   virtual void process(MDPObsEpoch& obs)
   {
      const DayTime& t=epoch.begin()->second.time;

      if (!firstObs && t<prevTime)
      {
         if (debugLevel)
            cout << "Out of order data at " << t << endl;
         return;
      }

      if (epoch.size() > 0 && t != obs.time)
      {
         if (!thin || (static_cast<int>(t.DOYsecond()) % thinning) == 0)
         {
            if (firstObs)
            {
               roh.firstObs = t;
               rinexObsOutput << roh;
               firstObs=false;
            }

            RinexObsData rod;
            makeRinexObsData(roh, rod, epoch);
            rinexObsOutput << rod;
         }
         epoch.clear();
         prevTime = t;
      }
      epoch[obs.prn] = obs;
   } // end of process(MDPObsEpoch)


   virtual void process()
   {
      MDPHeader header;
      MDPNavSubframe nav;
      MDPObsEpoch obs;

      mdpInput >> header;
      switch (header.id)
      {
         case MDPNavSubframe::myId :
            mdpInput >> nav;
            process(nav);
            break;
            
         case MDPObsEpoch::myId :
            mdpInput >> obs;
            process(obs);
            break;
      }
      timeToDie = !mdpInput;
   }

   virtual void shutDown()
   {}

private:
   gpstk::RinexObsHeader roh;
   gpstk::RinexNavHeader rnh;
   MDPStream mdpInput;
   RinexObsStream rinexObsOutput;
   RinexNavStream rinexNavOutput;
   MDPEpoch epoch;

   typedef std::pair<gpstk::RangeCode, gpstk::CarrierCode> RangeCarrierPair;
   typedef std::pair<RangeCarrierPair, short> NavIndex;
   typedef std::map<NavIndex, gpstk::MDPNavSubframe> NavMap;
   NavMap ephData;
   std::map<NavIndex, gpstk::EphemerisPages> ephPageStore;
   std::map<NavIndex, gpstk::EngEphemeris> ephStore;

   bool thin;
   int thinning;
   bool firstObs;
   gpstk::DayTime prevTime;
   gpstk::CommandOptionWithAnyArg mdpFileOpt, navFileOpt, obsFileOpt;
   gpstk::CommandOptionWithAnyArg thinningOpt;
   gpstk::CommandOptionWithAnyArg c2Opt;
};


int main(int argc, char *argv[])
{
   try
   {
      MDP2Rinex crap(argv[0]);

      if (!crap.initialize(argc, argv))
         exit(0);

      crap.run();
   }
   catch (gpstk::Exception &exc)
   { cout << exc << endl; }
   catch (std::exception &exc)
   { cout << "Caught std::exception " << exc.what() << endl; }
   catch (...)
   { cout << "Caught unknown exception" << endl; }
}
