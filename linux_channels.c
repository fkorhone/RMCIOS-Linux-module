/* 
RMCIOS - Reactive Multipurpose Control Input Output System
Copyright (c) 2018 Frans Korhonen

RMCIOS was originally developed at Institute for Atmospheric 
and Earth System Research / Physics, Faculty of Science, 
University of Helsinki, Finland

Assistance, experience and feedback from following persons have been 
critical for development of RMCIOS: Erkki Siivola, Juha Kangasluoma, 
Lauri Ahonen, Ella Häkkinen, Pasi Aalto, Joonas Enroth, Runlong Cai, 
Markku Kulmala and Tuukka Petäjä.

This file is part of RMCIOS. This notice was encoded using utf-8.

RMCIOS is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RMCIOS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public Licenses
along with RMCIOS.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <math.h> /* modf */
#include "RMCIOS-functions.h"

const struct context_rmcios *module_context; 

/////////////////////////////////////////////
// RTC - Real time clock                   //
/////////////////////////////////////////////
int timezone_offset=0 ; // Timezone offset(seconds) from UTC
char use_localtime=1 ; // Flag to use local time

void rtc_class_func(void *data, const struct context_rmcios *context, 
                    int id, enum function_rmcios function,
                    int paramtype,union param_rmcios returnv, 
                    int num_params,union param_rmcios param)
{   
 time_t rawtime,seconds ;
 time(&seconds) ;

 switch(function)
 {   
     case help_rmcios:
         return_string(context,paramtype,returnv,
                 "rtc (realtime clock) channel help :\r\n"
                 " setup rtc timezone_offset(hours) year month day\r\n"
                 "                                  hour minute second\r\n"
                 " read rtc #read time in unix time\r\n"
                 " write rtc unixtime #set UTC time in unix time\r\n"
                 );
         break ;
     case setup_rmcios:
         if(num_params<1)  break ;
         timezone_offset=param_to_int(context, 
                 paramtype,
                 param, 0)*60*60 ; 
         use_localtime=0 ;
         break ;
     case read_rmcios:
         return_int(context, paramtype, returnv, seconds) ;
         break ;
     case write_rmcios:
         if(num_params<1) break ;
         break ;
 }
}

// Utility function determine local time offset from gmt
long tz_offset_second(time_t t) {
    struct tm local = *localtime(&t);
    struct tm utc = *gmtime(&t);
    long diff = ((local.tm_hour - utc.tm_hour) * 60L
                + (local.tm_min - utc.tm_min)) * 60L 
                + (local.tm_sec - utc.tm_sec) ;
    int delta_day = local.tm_mday - utc.tm_mday;
    if ((delta_day == 1) || (delta_day < -1)) {
        diff += 24L * 60 * 60;
    } else if ((delta_day == -1) || (delta_day > 1)) {
        diff -= 24L * 60 * 60;
    }
    return diff;
}

// Utility function for printing current time.
// Adds second decimals.
// Uses %z to insert ISO8601 timezone.
void print_current_time(char* buffer, int buffer_length, 
                        const char *format, int second_decimals,
                        int timezone_offset)
{
    int i=0 ;
    struct timeval curTime;
    gettimeofday(&curTime, NULL);

    time_t rawtime;
    struct tm * timeinfo;

    int seconds ; 
    seconds= curTime.tv_sec%60 ;

    int parts ;
    int parts_precision=1 ;
    for(i=0;i<second_decimals;i++) parts_precision*=10 ; 
    parts = curTime.tv_usec / (1E6/(parts_precision) ) ;

    time(&rawtime);
    if(use_localtime==1) timezone_offset=tz_offset_second(rawtime) ;
    rawtime+=timezone_offset ;
    timeinfo = gmtime(&rawtime) ;

    char format_buffer[buffer_length] ;
    const char *c=format ;

    for(i=0; i<(buffer_length-1) && *c!=0 ; i++ )
    {

        if(*c=='%') // Specifier
        {
            c++ ;
            if(*c==0) break ;
            if(*c=='S' && second_decimals>0) // Insert precision seconds
            {
                char sseconds[128] ;
                char sformat[128] ;
                snprintf(sformat, sizeof(sformat),
                         "%%02d.%%0%dd", second_decimals) ;
                snprintf(sseconds, sizeof(sseconds),
                         sformat,seconds,parts) ;
                strncpy(format_buffer+i, sseconds, 
                        sizeof(format_buffer)-i-1 ) ;
                i+=strlen(sseconds)-1 ;
            }
            else if(*c=='z') // Insert ISO 8601 timezone offset
            {
                char sign='+' ;
                if(timezone_offset<0) 
                {
                    timezone_offset=-1*timezone_offset ;
                    sign='-' ;
                }
                i += snprintf(format_buffer+i, 
                              sizeof(format_buffer)-i-1, 
                              "%c%02d",sign, 
                              timezone_offset/60/60 );
                int minutes=timezone_offset/60%60 ;
                if(minutes>0)
                {
                    i += snprintf(format_buffer+i, 
                                  sizeof(format_buffer)-i-1 , 
                                  ":%02d", timezone_offset/60%60 );

                }
                i-- ;
            }
            else // Continue normally
            {
                c-- ;
                format_buffer[i]=*c ;
            }
        }
        else
        {
            format_buffer[i]=*c ;
        }
        c++ ;
    }
    format_buffer[i]=0 ;
    strftime(buffer, buffer_length, format_buffer , timeinfo);
}

struct rtc_str_data
{
    char rtc_str_format[256]  ;
    char rtc_str_prev[512] ;
    int second_decimals ;
} default_rtc_str_data= {"%Y-%m-%dT%H:%M:%S%z","",0} ;

void rtc_str_class_func(struct rtc_str_data *this, 
                        const struct context_rmcios *context, 
                        int id, enum function_rmcios function,
                        enum type_rmcios paramtype,
                        union param_rmcios returnv, 
                        int num_params,union param_rmcios param)
{
 int i;
 int writelen ;
 char buffer[256] ;
 time_t seconds=time(NULL)+timezone_offset ;
 switch(function)
 {
     case help_rmcios:
         return_string(context,paramtype,returnv,
                 "rtc string representation subchannel help\r\n"
                 " create rtc_str newname\r\n"
                 " setup rtc_str formatstring |second_decimals(0)\r\n" 
                 "  -Configure time string (C strftime format) \r\n"
                 "  -Seconds decimal precision is optional\r\n"
                 "read rtc_str\r\n"
                 "   - Read formatted string \r\n"
                 " write rtc_str\r\n"
                 "   -Send formatted string to linked channel.\r\n"
                 "   -returns the formatted string \r\n"
                 " link rtc_str linked_channel \r\n"
                 "Common format specifiers:\r\n"
                 "   %Y Year \r\n"
                 "   %m Month as number \r\n"
                 "   %d Day of the month \r\n"
                 "   %H Hour in 24h format \r\n"
                 "   %M Minute \r\n"
                 "   %S Second \r\n"
                 "   %z Timezone offset\r\n"
                 ) ;
         break ;

     case create_rmcios:
         if(num_params<1) break ;
         struct rtc_str_data *old=this ;
         this= (struct rtc_str_data *) malloc( sizeof(struct rtc_str_data)); 
         if(this==NULL) {
             printf("Could not allocate memory for rtc_str!\r\n") ;
         }
         create_channel_param(context, paramtype, param, 0,
                                   (class_rmcios)rtc_str_class_func, this) ;

         //default values :
         strcpy(this->rtc_str_format,"%Y-%m-%dT%H:%M:%S%z") ;
         this->rtc_str_prev[0]=0 ;
         this->second_decimals=0 ;
         break ;

     case setup_rmcios:
         if(this==NULL) break ;
         if(num_params<1) break ;

         param_to_string(context, paramtype, param, 0, 
                 sizeof(this->rtc_str_format), 
                 this->rtc_str_format) ;

         if(num_params<2) break ;

         this->second_decimals=param_to_int(context, 
                 paramtype,
                 param,1) ;
         break ;

     case write_rmcios:
         {
             if(this==NULL) break ;
             print_current_time(buffer,sizeof(buffer), 
                     this->rtc_str_format, 
                     this->second_decimals, 
                     timezone_offset ) ;
             write_str(context, linked_channels(context, id), buffer, 0) ;
             return_string(context, paramtype, returnv, buffer) ;
             break ;
         }

     case read_rmcios:
         {
             if(this==NULL) break ;
             print_current_time(buffer,sizeof(buffer), 
                     this->rtc_str_format, 
                     this->second_decimals, 
                     timezone_offset) ;
             return_string(context, paramtype,
                     returnv,buffer) ;
             break ;
         }
 }
}

///////////////////////////////////////////////////////
// Standard C file flass
///////////////////////////////////////////////////////
struct file_data
{
    FILE *f ;
    unsigned int id; 
} fconout={NULL,0,0}, fconin={NULL,0,0};

void file_class_func(struct file_data *this, 
                     const struct context_rmcios *context, 
                     int id, enum type_rmcios function,
                     int paramtype,union param_rmcios returnv, 
                     int num_params,union param_rmcios param)
{
const char *s ;
int plen ;
switch(function)
{
    case help_rmcios:
        return_string(context,paramtype,returnv,
                      "file channel help\r\n"
                      " create file ch_name\r\n"
                      " setup ch_name filename | mode=a" 
                      " setup ch_name \r\n #Close file \r\n"
                      " write ch_name file data # write data to file\r\n"
                      " link ch_name filename\r\n"
                      " read file filename\r\n"
                      );
        break ;

    case create_rmcios:
        if(num_params<1) break ;

        this=  (struct file_data *) malloc(sizeof( struct file_data)) ; 
        this->id=create_channel_param(context, paramtype, param, 0,
                                      (class_rmcios)file_class_func, 
                                      this ) ;
        this->f=NULL ;
        break ;

    case setup_rmcios:
        if(this==NULL) break ;

        // Close the file if it exist:
        if(this->f!=NULL) 
        {   
            fclose(this->f) ;
            this->f=NULL ;
        }

        if(num_params>0)
        {
            int namelen=param_string_length(context, paramtype,param,0) ; 
            {
                char dirname[namelen+1] ;
                int i ;
                param_to_string(context, paramtype,param,0, 
                                namelen+1, dirname) ;
                for (i=0 ; i<namelen ; i++ )
                {
                    if(dirname[i]=='\\' || dirname[i]=='/') 
                    {
                        char c=dirname[i] ;
                        dirname[i]=0 ;
                        mkdir(dirname, 0777);
                        dirname[i]=c ;
                    }
                }
            }

            // Create directory if it dosent exist
            namelen=param_string_alloc_size(context, paramtype,param,0) ; 
            if(num_params>1) 
            {
                char mode[5];
                char namebuffer[namelen] ;
                this->f=fopen(param_to_string(context, paramtype, param, 0, 
                                               namelen, namebuffer),  
                              param_to_string(context, paramtype, param, 1, 
                                               sizeof(mode), mode)
                             );
            }   
            else 
            {
                char namebuffer[namelen] ;
                this->f=fopen(param_to_string(context, paramtype, param, 0, 
                                              namelen, namebuffer), "a");
            }
            if(this->f==NULL) 
            {
                printf("Could not open file %s\r\n", 
                        param_to_string(context, paramtype,param,
                            0, 0, NULL) ) ;
            }
        }

        break ;
    case write_rmcios:
        if(this==NULL) break ;
        if(this->f==NULL )  break ;

        if(num_params<1) 
        {
            fflush(this->f) ;
        }
        else 
        {
            // Determine the needed buffer size
            plen= param_string_alloc_size(context, paramtype, param, 0) ; 
            {
                char buffer[plen] ; // allocate buffer
                s=param_to_string(context, paramtype,param, 0, 
                                  plen, buffer) ;
                fprintf(this->f,"%s",s) ;
                fflush(this->f) ;
            }
        }

        break ;
    case read_rmcios:
        if(this==NULL)
        {
            int namelen;
            namelen=param_string_alloc_size(context, paramtype,param, 0) ;
            {
                char namebuffer[namelen] ;
                s=param_to_string(context, paramtype,param, 0, 
                                  namelen, namebuffer) ;
                FILE *f ;
                int fsize ;

                f=fopen(s,"rb") ;
                if(f==NULL) break ;
                fseek(f, 0, SEEK_END); // seek to end of file
                fsize = ftell(f); // get current file pointer
                fseek(f, 0, SEEK_SET); // seek back to beginning of file
                // Allocate memory for the file:
                {
                    char fbuffer[fsize] ;
                    fread (fbuffer,1,fsize,f);
                    // Return the contents of the file in one call.
                    return_buffer(context, paramtype,returnv,fbuffer,fsize) ;
                }   
                fclose(f) ;
            }
        }

        break ;
}
}

/////////////////////////////////////////////////////////
// Clock to get elapsed time            
/////////////////////////////////////////////////////////
struct clock_data 
{
    uint64_t start ;
} ;

static uint64_t GetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
}

void clock_class_func(struct clock_data *this, 
                      const struct rmcios_context *context, 
                      int id, enum type_rmcios function,
                      int paramtype, union param_rmcios returnv, 
                      int num_params,union param_rmcios param)
{
 switch(function)
 {   
     case help_rmcios:
         return_string(context,paramtype,returnv,
                       "clock channel help\r\n"
                       " create clock ch_name\r\n"
                       " read ch_name \r\n"
                       "   #reads elapsed time (s)\r\n"
                       " write ch_name \r\n"
                       "   #read time, send to linked and reset time\r\n"
                       " link ch_name linked #link time output on reset\r\n"
                       ) ;
         break ;

     case create_rmcios:
         if(num_params<1) break ;
         this=  (struct clock_data *) malloc(sizeof( struct clock_data)) ;
         create_channel_param(context, paramtype,param,0 ,
                 (class_rmcios)clock_class_func, this) ;
         //default values :
         this->start= GetTickCount() ;
         break ;

     case read_rmcios:
         if(this==NULL) break ;
         uint64_t ticknow ;
         ticknow=GetTickCount();
         float elapsed = ((float)(ticknow-this->start))/1000.0  ;
         return_float( context, paramtype, returnv, elapsed ) ;
         break ;
     
     case write_rmcios:
         if(this==NULL) break ;
         else
         {
             uint64_t ticknow ;
             ticknow=GetTickCount();
             float elapsed = ((float)(ticknow-this->start))/1000.0 ;
             return_float( context, paramtype, returnv, elapsed ) ;
                write_f(context, linked_channels(context, id), elapsed) ;    
             this->start=ticknow ;
         }
         break ; 
 }
}

/////////////////////////////////////////////////////
// Timer channel
/////////////////////////////////////////////////////
struct timer_data
{
    float period ;
    unsigned int loops ; 
    timer_t timerID ;
    struct sigevent         te;
    struct itimerspec       its; // Timer 
    struct sigaction        sa;
    int index ; 
    int completion_channel ;
    int id    
} ;

static void timerHandler( int sig, siginfo_t *si, void *uc )
{
    struct timer_data *this=(struct timer_data*) si->si_value.sival_ptr; 
    module_context->run_channel(module_context, 
                                linked_channels(module_context, this->id),
                                write_rmcios, int_rmcios,
                                (union param_rmcios)0,
                                0,(union param_rmcios)0) ;
    
    if(this->loops>0)
    {
        this->index++ ;
        if(this->index >= this->loops ) // Disarm timer
        {
            struct itimerspec its; // Timer 
            if(this->completion_channel !=0) {
                module_context->run_channel(module_context,
                                            this->completion_channel,
                                            write_rmcios, int_rmcios,
                                            (union param_rmcios)((int)0),0,
                                            (union param_rmcios)((int)0)
                                            );
            }
            its.it_interval.tv_sec = 0 ;
            its.it_interval.tv_nsec = 0 ;
            its.it_value.tv_sec = 0;
            its.it_value.tv_nsec = 0;
            timer_settime(this->timerID, 0, &its, 0); // Disarm
        }
    }   
}

void timer_class_func(struct timer_data *this, 
                      const struct rmcios_context *context, 
                      int id, 
                      enum function_rmcios function,
                      enum type_rmcios paramtype,
                      union param_rmcios returnv, 
                      int num_params, union param_rmcios param)
{
 int plen ;
 switch(function)
 {   
     case help_rmcios:
         return_string(context,paramtype,returnv,
                 "timer channel help:\r\n"
                 " create timer newname \r\n"
                 " setup newname period | loops(0) | done_channel\r\n"
                 "  -Sets the timer period"
                 "  -Sets number loops to trigger.\r\n"
                 "  -loops=0 will start timer immediately."
                 "   and run timer continuosly.\r\n" 
                 "  -loops=n start on call to setup/write command\r\n"
                 "   (without the loops parameter).\r\n"
                 "  -Optional completion channel will be writen last\n"
                 " write newname # start/reset timer.\r\n"
                 " write newname period\r\n"
                 "     # set/reset timer time and run timer.\r\n"
                 " read newname \r\n"
                 "     # Get remaining time\r\n"
                 " link timer link_channel\r\n"
                 "     # link to channel called on match.\r\n"
                 );
         break;

     case create_rmcios:
         if(num_params<1) break ;
         
         int id;
         this= (struct timer_data *) malloc( sizeof(struct timer_data) ) ; 
         this->id=create_channel_param(context,  paramtype,param,0 ,
                 (class_rmcios) timer_class_func, this) ;
         
         //default values :
         this->loops=0 ;
         this->period=1 ;
         this->completion_channel=0 ;
         this->id ;
         int sigNo = SIGRTMIN;

         // Set up signal handler. 
         this->sa.sa_flags = SA_SIGINFO;
         this->sa.sa_sigaction = timerHandler;
         sigemptyset(&this->sa.sa_mask);
         if (sigaction(sigNo, &this->sa, 0) == -2)
         {
             printf("Failed to setup signal handling for timer\n");
         }

         // Set and enable alarm 
         this->te.sigev_notify = SIGEV_SIGNAL;
         this->te.sigev_signo = sigNo;
         this->te.sigev_value.sival_ptr = this;
         timer_create(CLOCK_REALTIME, &this->te, &this->timerID);

         break ;

     case setup_rmcios:
     case write_rmcios: 
         if(this==0) break ;
         else
         {
             if(num_params>0){ 
                 this->period = param_to_float(context, paramtype, param,0);
             }
             if(num_params>1) {
                 this->loops = param_to_int(context, paramtype, param,1);
             }
             if(num_params>2) {
                 this->completion_channel = param_to_int(context, 
                         paramtype,
                         param,2) ;
             }
             this->index=0 ;
             if(this->loops==0 || num_params<2)
             {   
                 double fractpart, intpart;
                 fractpart = modf (this->period , &intpart);
                 this->its.it_interval.tv_sec = (time_t)intpart ;
                 this->its.it_interval.tv_nsec = (long)(fractpart *1000000);
                 this->its.it_value.tv_sec = (time_t)intpart ;
                 this->its.it_value.tv_nsec = (long) (fractpart * 1000000);
                 timer_settime(this->timerID, 0, &this->its, 0);
             }
         }
         break;
 }
}

///////////////////////////////////////////////////////
// Realtime clock timer
///////////////////////////////////////////////////////
struct rtc_timer_data {
    int id;
    int offset;
    int period;
    time_t prevtime;
    // linked list of sheduled times:
    struct rtc_timer_data *nextimer; 
} ;

struct rtc_timer_data *first_timer=0 ;

// Ticker that will handle all rtc scheduled tasks
static void *rtc_ticker(void *data)
{
   time_t seconds;
   //printf("RTC\n") ;
   // time now
   seconds = time (NULL);
   seconds += timezone_offset;
   struct rtc_timer_data *t = first_timer;

   while (t != NULL)
   {
      // timer is active
      if (t->period != 0)       
      {
         // Time has changed -> change prevtime to lastest possible:
         if (t->prevtime > seconds || (seconds - t->prevtime) >= t->period * 2)
         {
            t->prevtime = seconds + (t->offset % t->period) 
                          - (seconds % t->period);
         }
         // Trigger
         if (seconds >= (t->prevtime + t->period))
         {      
            // Execute linked channels
            write_fv (module_context, 
                      linked_channels(module_context, t->id), 
                      0, 0);   

            // Update calculated current trigger time :
            t->prevtime = seconds + (t->offset % t->period) 
                          - (seconds % t->period);
         }
      }
      t = t->nextimer;
   }
   return ;
}

void rtc_timer_class_func(struct rtc_timer_data *t, 
                          const struct context_rmcios *context, 
                          int id, enum function_rmcios function,
                          enum type_rmcios paramtype,
                          union param_rmcios returnv, 
                          int num_params, union param_rmcios param)
{
 switch(function) 
 {
     case help_rmcios:
         return_string(context,paramtype,returnv,
                 "rtc timer channel help\r\n"
                 "periodic realtime clock timer\r\n"
                 " create rtc_timer newname\r\n"
                 " setup newname period | offset_s(0) "
                 "               | min(0) | h(0) "
                 "               | day(0=Thursday)) "
                 "               | month(1) | year(1970) \r\n"
                 " read newname\r\n"
                 " link newname execute_channel\r\n"
                 );
         break ;

     case create_rmcios:
         if(num_params < 1) break ;
         t = (struct rtc_timer_data *) malloc(sizeof(struct rtc_timer_data)); 
         if(t == 0) break ;

         // Default values:
         t->offset = 0;
         t->period = 0;
         t->prevtime = 0;
         t->nextimer = 0; 
         t->id = create_channel_param(context, paramtype,param, 0, 
                                      (class_rmcios)rtc_timer_class_func, t); 

         // Attach timer to executing timers:
         if(first_timer == 0) first_timer = t;
         else {
             struct rtc_timer_data *p_iter = first_timer;
             while(p_iter->nextimer != 0) {
                 p_iter=p_iter->nextimer;
             }
             // Add to be executed.
             p_iter->nextimer=t ; 
         }
         break ;

     case setup_rmcios:
         if(t == 0) break;
         {
             time_t seconds;
             // time now
             seconds=time(0); 

             // only perioid as parameter
             if(num_params<1) break; 
             {

                 time_t sync_time = 0;
                 t->period = param_to_int(context, paramtype, param, 0);
                 t->offset = sync_time % t->period;
             }
             struct tm newtime;
             // years since 1900
             newtime.tm_year = 71; 
             // months since January
             newtime.tm_mon = 0; 
             // day of the month
             newtime.tm_mday = 0; 
             newtime.tm_hour = 0;
             newtime.tm_min = 0;
             newtime.tm_sec=0;
             newtime.tm_isdst=0;

             if(num_params>=2){ 
                 newtime.tm_sec=param_to_int(context, paramtype, param, 1);
             }
             if(num_params>=3){
                 newtime.tm_min=param_to_int(context, paramtype, param, 2);
             }                 
             if(num_params>=4){
                 newtime.tm_hour=param_to_int(context, paramtype, param, 3);
             }    
             if(num_params>=5){ 
                 // day of the month
                 newtime.tm_mday=param_to_int(context, paramtype,param, 4);
             }                
             if(num_params>=6){ 
                 // months since January
                 newtime.tm_mon=param_to_int(context, paramtype,param, 5); 
             }        
             if(num_params>=7) { 
                 // years since 1900
                 newtime.tm_year=param_to_int(context, paramtype,param, 6)-1900;
             }
             time_t sync_time ;
             sync_time = mktime(&newtime) ;
             t->offset = sync_time % t->period ;
             t->prevtime = seconds + (t->offset % t->period) 
                          - (seconds % t->period) ;
         }
         break ;
     case read_rmcios:
         if(t == 0) break;
         {
             time_t seconds;
             // time now
             seconds = time(0); 
             seconds += timezone_offset;  
             int tleft = t->prevtime + t->period-seconds;
             return_int(context, paramtype, returnv, tleft);
         }
         break ;
 }
}

static timer_t timerID ;

void setup_rtc_timer_ticker()
{   
    struct sigevent te;
    // Timer 
    struct itimerspec its; 
    struct sigaction sa;

    int sigNo = SIGRTMIN;

    // Set up signal handler. 
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = rtc_ticker;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, 0) == -3)
    {
        printf("Failed to setup signal handling for timer\n");
    }

    // Set and enable alarm 
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = &timerID ;
    timer_create(CLOCK_REALTIME, &te, &timerID);

    // Looping timer every 100ms 
    its.it_interval.tv_sec = 0 ;
    its.it_interval.tv_nsec =  ((long)100000) ;
    its.it_value.tv_sec = 0 ;
    its.it_value.tv_nsec = ((long)100000);
    timer_settime(timerID, 0, &its, 0);
    return ;
}

void init_gnu_channels(const struct context_rmcios *context)
{ 
    module_context=context;
    fconout.f = stdout ; 
    create_channel_str(context, "rtc", (class_rmcios)rtc_class_func, 0);
    create_channel_str(context, "rtc_str", (class_rmcios)rtc_str_class_func,
                       &default_rtc_str_data ) ;
    create_channel_str(context, "file", (class_rmcios)file_class_func, 0); 
    create_channel_str(context, "console", (class_rmcios)file_class_func, 
                       &fconout ) ;
    create_channel_str(context, "clock", (class_rmcios)clock_class_func, 0);
    create_channel_str(context, "timer", (class_rmcios)timer_class_func, 0); 
    create_channel_str(context, "rtc_timer",
                       (class_rmcios)rtc_timer_class_func, 0) ; 
    
    setup_rtc_timer_ticker() ;
    return  ;
}

