#include "rutil/compat.hxx"

#include <string.h>
#include <ctype.h>
#include <time.h>

#include "resip/stack/DateCategory.hxx"

#include "resip/stack/Transport.hxx"
#include "rutil/Data.hxx"
#include "rutil/DnsUtil.hxx"
#include "rutil/Logger.hxx"
#include "rutil/ParseBuffer.hxx"
#include "rutil/Socket.hxx"
//#include "rutil/WinLeakCheck.hxx"  // not compatible with placement new used below

using namespace resip;
using namespace std;

namespace resip
{
// Implemented in gen/DayOfWeekHash.cxx
struct days { const char *name; DayOfWeek type; };
class DayOfWeekHash
{
private:
  static inline unsigned int hash (const char *str, GPERF_SIZE_TYPE len);
public:
  static const struct days *in_word_set (const char *str, GPERF_SIZE_TYPE len);
};

// Implemented in gen/MonthHash.cxx
struct months { const char *name; Month type; };
class MonthHash
{
private:
  static inline unsigned int hash (const char *str, GPERF_SIZE_TYPE len);
public:
  static const struct months *in_word_set (const char *str, GPERF_SIZE_TYPE len);
};
}

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

//====================
// Date
//====================

Data resip::DayOfWeekData[] =
{
   "Sun",
   "Mon",
   "Tue",
   "Wed",
   "Thu",
   "Fri",
   "Sat"
};

Data resip::MonthData[] =
{
   "Jan",
   "Feb",
   "Mar",
   "Apr",
   "May",
   "Jun",
   "Jul",
   "Aug",
   "Sep",
   "Oct",
   "Nov",
   "Dec"
};


DateCategory::DateCategory(TmType tmtype)
   : ParserCategory(),
     mDayOfWeek(Sun),
     mDayOfMonth(),
     mMonth(Jan),
     mYear(0),
     mHour(0),
     mMin(0),
     mSec(0)
   , mMillisec(0)
   , mDateType(tmtype)
{
   time_t now;
   time(&now);
   if (now == ((time_t)-1))
   {
      int e = getErrno();
      DebugLog (<< "Failed to get time: " << strerror(e));
      Transport::error(e);
      return;
   }
   
   setDatetime(now);
}

DateCategory::DateCategory(time_t datetime, TmType tmtype)
   : ParserCategory(),
     mDayOfWeek(Sun),
     mDayOfMonth(),
     mMonth(Jan),
     mYear(0),
     mHour(0),
     mMin(0),
     mSec(0)
   , mMillisec(0)
   , mDateType(tmtype)
{
   setDatetime(datetime);
}

DateCategory::DateCategory(const HeaderFieldValue& hfv, 
                           Headers::Type type,
                           PoolBase* pool, TmType tmtype)
   : ParserCategory(hfv, type, pool),
     mDayOfWeek(Sun),
     mDayOfMonth(),
     mMonth(Jan),
     mYear(0),
     mHour(0),
     mMin(0),
     mSec(0)
   , mMillisec(0)
   , mDateType(tmtype)
{}

DateCategory::DateCategory(const DateCategory& rhs,
                           PoolBase* pool)
   : ParserCategory(rhs, pool),
     mDayOfWeek(rhs.mDayOfWeek),
     mDayOfMonth(rhs.mDayOfMonth),
     mMonth(rhs.mMonth),
     mYear(rhs.mYear),
     mHour(rhs.mHour),
     mMin(rhs.mMin),
     mSec(rhs.mSec)
   , mMillisec(rhs.mMillisec)
   , mDateType(rhs.mDateType)
{}

DateCategory&
DateCategory::operator=(const DateCategory& rhs)
{
   if (this != &rhs)
   {
      ParserCategory::operator=(rhs);
      mDayOfWeek = rhs.mDayOfWeek;
      mDayOfMonth = rhs.mDayOfMonth;
      mMonth = rhs.mMonth;
      mYear = rhs.mYear;
      mHour = rhs.mHour;
      mMin = rhs.mMin;
      mSec = rhs.mSec;
      mMillisec = rhs.mMillisec;
      mDateType = rhs.mDateType;
   }
   return *this;
}

bool 
DateCategory::setDatetime(time_t datetime)
{
   struct tm gmt;
   struct tm* gmtp = NULL;
#if defined(WIN32) || defined(__sun)
   if(mDateType == reSIPDate)
      gmtp = gmtime(&datetime);
   else
      gmtp = localtime(&datetime);
   if (gmtp == 0)
   {
        int e = getErrno();
        DebugLog (<< "Failed to convert to gmt: " << strerror(e));
        Transport::error(e);
        return false;
   }
   memcpy(&gmt,gmtp,sizeof(gmt));
#else
   if (mDateType == reSIPDate)
   {
      gmtp = gmtime_r(&datetime, &gmt);
   }
   else
   {
      gmtp = localtime_r(&datetime, &gmt);
   }
   if (gmtp == NULL)
   {
      int e = getErrno();
      DebugLog(<< "Failed to convert to gmt: " << strerror(e));
      Transport::error(e);
      return false;
   }
#endif

   mDayOfWeek = static_cast<DayOfWeek>(gmt.tm_wday);
   mDayOfMonth = gmt.tm_mday;
   mMonth = static_cast<Month>(gmt.tm_mon);
   mYear = gmt.tm_year + 1900;
   mHour = gmt.tm_hour;
   mMin = gmt.tm_min;
   mSec = gmt.tm_sec;
   DebugLog (<< "Set date: day=" << mDayOfWeek 
             << " month=" << mMonth
             << " year=" << mYear
             << " " << mHour << ":" << mMin << ":" << mSec);
   return true;
}

DayOfWeek
DateCategory::DayOfWeekFromData(const Data& dow)
{
   const char *str = dow.data();
   Data::size_type len = dow.size();

   const struct days* _day = DayOfWeekHash::in_word_set(str, len);
   if(_day != 0)
   {
      return _day->type;
   }
   else
   {
      return Sun;
   }
}

Month
DateCategory::MonthFromData(const Data& mon)
{
   const char *str = mon.data();
   Data::size_type len = mon.size();

   const struct months* _month = MonthHash::in_word_set(str, len);
   if(_month != 0)
   {
      return _month->type;
   }
   else
   {
      return Jan;
   }
}

const DayOfWeek& 
DateCategory::dayOfWeek() const 
{
   checkParsed();
   return mDayOfWeek;
}

int&
DateCategory::dayOfMonth() 
{
   checkParsed();
   return mDayOfMonth;
}

int 
DateCategory::dayOfMonth() const 
{
   checkParsed();
   return mDayOfMonth;
}

Month& 
DateCategory::month() 
{
   checkParsed();
   return mMonth;
}

Month
DateCategory::month() const
{
   checkParsed();
   return mMonth;
}

int& 
DateCategory::year() 
{
   checkParsed();
   return mYear;
}

int
DateCategory::year() const 
{
   checkParsed();
   return mYear;
}

int&
DateCategory::hour() 
{
   checkParsed();
   return mHour;
}

int
DateCategory::hour() const 
{
   checkParsed();
   return mHour;
}

int&
DateCategory::minute() 
{
   checkParsed();
   return mMin;
}

int
DateCategory::minute() const 
{
   checkParsed();
   return mMin;
}

int&
DateCategory::second() 
{
   checkParsed();
   return mSec;
}

int
DateCategory::second() const 
{
   checkParsed();
   return mSec;
}

void
DateCategory::parse(ParseBuffer& pb)
{
   if (mDateType == reSIPDate)
   {
      // Mon, 04 Nov 2002 17:34:15 GMT
      // Note: Day of Week is optional, so this is also valid: 04 Nov 2002 17:34:15 GMT

      const char* anchor = pb.skipWhitespace();

      // If comma is present, then DayOfWeek is present
      pb.skipToChar(Symbols::COMMA[0]);
      if (!pb.eof())
      {
         Data dayOfWeek;
         pb.data(dayOfWeek, anchor);
         mDayOfWeek = DateCategory::DayOfWeekFromData(dayOfWeek);

         pb.skipChar(Symbols::COMMA[0]);
      }
      else
      {
         pb.reset(pb.start());
         mDayOfWeek = DayOfWeek::NA;
      }

      pb.skipWhitespace();

      mDayOfMonth = pb.integer();

      anchor = pb.skipWhitespace();
      pb.skipNonWhitespace();

      Data month;
      pb.data(month, anchor);
      mMonth = DateCategory::MonthFromData(month);

      pb.skipWhitespace();
      mYear = pb.integer();

      pb.skipWhitespace();

      mHour = pb.integer();
      pb.skipChar(Symbols::COLON[0]);
      mMin = pb.integer();
      pb.skipChar(Symbols::COLON[0]);
      mSec = pb.integer();

      pb.skipWhitespace();
      pb.skipChar('G');
      pb.skipChar('M');
      pb.skipChar('T');

      pb.skipWhitespace();
      pb.assertEof();
   }
   else
   {
      //Date: 2022-08-24T14:27:00.250
      const char* anchor = pb.skipWhitespace();
      mYear = pb.integer();
      pb.skipChar(Symbols::DASH[0]);

      mMonth = (Month)pb.integer();
      pb.skipChar(Symbols::DASH[0]);

      mDayOfMonth = pb.integer();
      pb.skipChar('T');

      mHour = pb.integer();
      pb.skipChar(Symbols::COLON[0]);

      mMin = pb.integer();
      pb.skipChar(Symbols::COLON[0]);

      mSec = pb.integer();

      if (!pb.eof())
      {
         pb.skipChar(Symbols::DOT[0]);
         mMillisec = pb.integer();
      }
   }
}

ParserCategory* 
DateCategory::clone() const
{
   return new DateCategory(*this);
}

ParserCategory* 
DateCategory::clone(void* location) const
{
   return new (location) DateCategory(*this);
}

ParserCategory* 
DateCategory::clone(PoolBase* pool) const
{
   return new (pool) DateCategory(*this, pool);
}

static void pad2(const int x, EncodeStream& str)
{
   if (x < 10)
   {
      str << Symbols::ZERO[0];
   }
   str << x;
}
static void pad3(const int x, EncodeStream& str)
{
   if (x < 100)
   {
      str << Symbols::ZERO[0];
      if (x < 10)
      {
         str << Symbols::ZERO[0];
      }
   }
   str << x;
}

EncodeStream& 
DateCategory::encodeParsed(EncodeStream& str) const
{
   if (mDateType == reSIPDate)
   {
      if (mDayOfWeek != DayOfWeek::NA)
      {
         str << DayOfWeekData[mDayOfWeek] // Mon
            << Symbols::COMMA[0] << Symbols::SPACE[0];
      }

      pad2(mDayOfMonth, str);  //  04

      str << Symbols::SPACE[0]
         << MonthData[mMonth] << Symbols::SPACE[0] // Nov
         << mYear << Symbols::SPACE[0]; // 2002

      pad2(mHour, str);
      str << Symbols::COLON[0];
      pad2(mMin, str);
      str << Symbols::COLON[0];
      pad2(mSec, str);
      str << " GMT";
   }
   else
   {
      pad2(mYear, str);
      str << Symbols::DASH[0];
      pad2(mMonth + 1, str);
      str << Symbols::DASH[0];
      pad2(mDayOfMonth, str);
      str << 'T';
      pad2(mHour, str);
      str << Symbols::COLON[0];
      pad2(mMin, str);
      str << Symbols::COLON[0];
      pad2(mSec, str);
      str << Symbols::DOT[0];
      pad3(0, str);
   }
   

   return str;
}


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */
