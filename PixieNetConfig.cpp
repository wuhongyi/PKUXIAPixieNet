/*----------------------------------------------------------------------
 * Copyright (c) 2017 XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of XIA LLC
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *----------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/stat.h>//stat(const char *file_name,struct stat *buf)
#include <termios.h> // tcgetattr(), tcsetattr()
#include <sys/time.h> // struct timeval, select()

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip> 
#include <vector>

#include "PixieNetDefs.h"
#include "PixieNetConfig.h"

#include "wuReadData.hh"

using namespace std;


namespace {
  
  void split( std::vector<std::string> &resutls,
             const std::string &input, const char *delims )
  {
    resutls.clear();
    
    size_t prev_delim_end = 0;
    size_t delim_start = input.find_first_of( delims, prev_delim_end );
    
    while( delim_start != std::string::npos )
    {
      if( (delim_start-prev_delim_end) > 0 )
        resutls.push_back( input.substr(prev_delim_end,(delim_start-prev_delim_end)) );
      
      prev_delim_end = input.find_first_not_of( delims, delim_start + 1 );
      if( prev_delim_end != std::string::npos )
        delim_start = input.find_first_of( delims, prev_delim_end + 1 );
      else
        delim_start = std::string::npos;
    }//while( this_pos < input.size() )
    
    if( prev_delim_end < input.size() )
      resutls.push_back( input.substr(prev_delim_end) );
  }//split(...)
  
  
 
  
  bool starts_with( const std::string &line, const std::string &label ){
    const size_t len1 = line.size();
    const size_t len2 = label.size();
    
    if( len1 < len2 )
      return false;
    
    return (line.substr(0,len2) == label);
  }//istarts_with(...)
  
  
  // trim from start
  static inline std::string &ltrim(std::string &s)
  {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    
    if( s.size() )
    {
      const size_t pos = s.find_first_not_of( '\0' );
      if( pos != 0 && pos != string::npos )
        s.erase( s.begin(), s.begin() + pos );
      else if( pos == string::npos )
        s.clear();
    }
    
    return s;
  }
  
  // trim from end
  static inline std::string &rtrim(std::string &s)
  {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    
    const size_t pos = s.find_last_not_of( '\0' );
    if( pos != string::npos && (pos+1) < s.size() )
      s.erase( s.begin() + pos + 1, s.end() );
    else if( pos == string::npos )
      s.clear();  //string is all '\0' characters
    
    return s;
  }
  
  // trim from both ends
  void trim( std::string &s )
  {
    ltrim( rtrim(s) );
  }//trim(...)
  
  bool split_label_values( const string &line, string &label, string &values )
  {
    label.clear();
    values.clear();
    const size_t pos = line.find_first_of( " \t,;" );
    if( !pos || pos == string::npos )
      return false;
    label = line.substr( 0, pos );
    values = line.substr( pos );
    trim( values );
    
    return true;
  }
  
  std::istream &safe_get_line( std::istream &is, std::string &t, const size_t maxlength )
  {
    //adapted from  http://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
    t.clear();
    
    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.
    std::istream::sentry se( is, true );
    std::streambuf *sb = is.rdbuf();
    
    for( ; !maxlength || (t.length() < maxlength); )
    {
      int c = sb->sbumpc(); //advances pointer to current location by one
      switch( c )
      {
        case '\r':
          c = sb->sgetc();  //does not advance pointer to current location
          if(c == '\n')
            sb->sbumpc();   //advances pointer to one current location by one
          return is;
        case '\n':
          return is;
        case EOF:
          is.setstate( ios::eofbit );
          return is;
        default:
          t += (char)c;
      }//switch( c )
    }//for(;;)
    
    return is;
  }//safe_get_line(...)
  
  int get_single_value_str( const map<string,string> &label_to_values,
                            const string &label, string &value, int ignore_missing )
  // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
  {
    const map<string,string>::const_iterator pos = label_to_values.find( label );
    if( pos == label_to_values.end() )
    {
      if(ignore_missing==1)   {
            cerr << label << " ";
      } else {
            cerr << "Parameter '" << label << "' was not in config file" << endl;
      }
      return 1;
    }
    
    value = pos->second;
    trim( value );//去掉字符序列左边和右边的空格
    
    vector<string> fields;
    split( fields, value, " \t,;" );
    if( fields.size() != 1 )
    {
      cerr << "Parameter '" << label << "' had " << fields.size() << " values\n";
      return -1;
    }
    
    return 0;
  }
  
  
  int get_multiple_value_str( const map<string,string> &label_to_values,
                            const string &label, string values[NCHANNELS], int ignore_missing )
   // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
 {
    const map<string,string>::const_iterator pos = label_to_values.find( label );
    if( pos == label_to_values.end() )
    {
      if(ignore_missing==1)   {
            cerr << label << " ";
      } else {
            cerr << "Parameter '" << label << "' was not in config file" << endl;
      }
      return 1;
    }
    
    string valuestr = pos->second;
    trim( valuestr );
    
    vector<string> fields;
    split( fields, valuestr, " \t,;" );
    if( fields.size() != NCHANNELS )
    {
      cerr << "Parameter '" << label << "' had " << fields.size()
           << " values, and not " << NCHANNELS << "\n";
      return -1;
    }
    
    for( int i = 0; i < NCHANNELS; ++i )
    {
      trim( fields[i] );
      values[i] = fields[i];
    }
    
    return 0;
  }
  
   int parse_single_bool_val( const map<string,string> &label_to_values,
                             const string &label, bool &value, int ignore_missing )
    // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
    {
    string valstr;
    int ret;
    ret =  get_single_value_str( label_to_values, label, valstr, ignore_missing); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;
    

    if( valstr == "true" || valstr == "1" )
    {
      value = true;
      return 0;
    }

    if( valstr == "false" || valstr == "0" )
    {
      value = false;
      return 0;
    }

    cerr << "Parameter '" << label << "' with value " << valstr
            << " could not be interpredted as a boolean\n";
    return -1;
  }//parse_single_bool_val(...)


  int parse_multiple_bool_val( const map<string,string> &label_to_values,
                            const string &label, bool values[NCHANNELS], int ignore_missing )
    // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
    {
    string valstrs[NCHANNELS];
    int ret;
    ret =  get_multiple_value_str( label_to_values, label, valstrs, ignore_missing ); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;
    
    for( int i = 0; i < NCHANNELS; ++i )
    {
      if( valstrs[i] == "true" || valstrs[i] == "1" )
        values[i] = true;
      else if( valstrs[i] == "false" || valstrs[i] == "0" )
        values[i] = false;
      else
      {
        cerr << "Parameter '" << label << "' with value '" << valstrs[i]
             << "' could not be interpredted as a boolean\n";
        return -1;
      }
    }
    
    return 0;
  }//parse_multiple_bool_val(...)


  int parse_single_int_val( const map<string,string> &label_to_values,
                             const string &label, unsigned int &value, int ignore_missing )
  // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
  {
    string valstr;
    int ret;
    ret =  get_single_value_str( label_to_values, label, valstr, ignore_missing); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;

    
    char *cstr = &valstr[0u];    // c++11 avoidance
    char *end;
    try
    {
      value = strtol(cstr, &end, 0);
      // value = std::stoul( valstr, nullptr, 0 );   // requires c++11
    }catch(...)
    {
       cerr << "Parameter '" << label << "' with value " << valstr
            << " could not be interpredted as an unsigned int\n";
      return -1;
    }
    
    return 0;
  }//parse_single_int_val(...)
  
  int parse_single_dbl_val( const map<string,string> &label_to_values,
                            const string &label, double &value, int ignore_missing )
   // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
   {
    string valstr;

    int ret;
    ret =  get_single_value_str( label_to_values, label, valstr, ignore_missing); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;

    
    char *cstr = &valstr[0u];    // c++11 avoidance
    char *end;
    try
    {
      value = strtod(cstr, &end);
      //value = std::stod( valstr );      // requires c++11
    }catch(...)
    {
      cerr << "Parameter '" << label << "' with value " << valstr
      << " could not be interpredted as an double\n";
      return -1;
    }
    
    return 0;
  }//parse_single_dbl_val(...)
  
  
  int parse_multiple_int_val( const map<string,string> &label_to_values,
                            const string &label, unsigned int values[NCHANNELS], int ignore_missing )
   // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
   {
    string valstrs[NCHANNELS];
    int ret;
    ret =  get_multiple_value_str( label_to_values, label, valstrs, ignore_missing ); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;

    char *cstr;    // c++11 avoidance
    char *end;
    string valstr;
    
    for( int i = 0; i < NCHANNELS; ++i )
    {
      try
      {
      valstr = valstrs[i];
      cstr = &valstr[0u];
      values[i] = strtol(cstr, &end, 0);
      //  values[i] = std::stoul( valstrs[i], nullptr, 0 );   // requires c++11
      }catch(...)
      {
        cerr << "Parameter '" << label << "' with value " << valstrs[i]
             << " could not be interpredted as an unsigned int\n";
        return -1;
      }
    }
    
    return 0;
  }//parse_multiple_int_val(...)
  
  
  int parse_multiple_dbl_val( const map<string,string> &label_to_values,
                              const string &label, double values[NCHANNELS], int ignore_missing )
   // returns 0 if value sucessfully updated, negative value if error, +1 if value not in file (sometimes ok)
   {
    string valstrs[NCHANNELS];
    int ret;
    ret =  get_multiple_value_str( label_to_values, label, valstrs, ignore_missing ); //  //  0 if valid, <0 if error, +1 not in file (sometimes ok)
    if( ret!=0 )
      return ret;

    char *cstr;    // c++11 avoidance
    char *end;
    string valstr;
    
    for( int i = 0; i < NCHANNELS; ++i )
    {
      try
      {
        valstr = valstrs[i];
        cstr = &valstr[0u];
        values[i] = strtod(cstr, &end);
        //values[i] = std::stod( valstrs[i] );     // requires c++11
      }catch(...)
      {
        cerr << "Parameter '" << label << "' with value " << valstrs[i]
        << " could not be interpredted as a double\n";
        return -1;
      }
    }
    
    return 0;
  }//parse_multiple_dbl_val(...)
  
  bool read_config_file_lines( const char * const filename,
                               map<string,string> &label_to_values )
  {
    string line;
    
    ifstream input( filename, ios::in | ios::binary );
    
    if( !input )
    {
      cerr << "Failed to open '" << filename << "'" << endl;
      return false;
    }
    
    while( safe_get_line( input, line, LINESZ ) )
    {
      trim( line );
      if( line.empty() || line[0] == '#' )
        continue;
      
      string label, values;
      const bool success = split_label_values( line, label, values );
      
      if( !success || label.empty() || values.empty() )
      {
        cerr << "Warning: encountered invalid config file line '" << line
        << "', skipping" << endl;
        continue;
      }
      
      label_to_values[label] = values;
    }///more lines in config file
    
    return true;
  }//read_config_file_lines(..)
  
}//namespace


int SetBit(int bit, int value)      // returns "value" with bit "bit" set
{
	return (value | (1<<bit) );
}

int ClrBit(int bit, int value)      // returns "value" with bit "bit" cleared
{
	value=SetBit(bit, value);
	return(value ^ (1<<bit) );
}

int SetOrClrBit(int bit, int value, int set)      // returns "value" with bit "bit" cleared or set, depending on "set"
{
	value=SetBit(bit, value);
   if(set)
      return(value);
   else
	   return(value ^ (1<<bit) );
}


int init_PixieNetFippiConfig_from_file( const char * const filename, 
                                        int ignore_missing,                     
                                        struct PixieNetFippiConfig *config )
{
   // if ignore_missing == 1, missing parameters (parse_XXX returns 1) are ok
   // this is set for a second pass, after filling parameters with defaults
  bool bit, bits[NCHANNELS];
  int ret;

  if(ignore_missing==1) 
  {
      cerr << "Using defaults for following parameters " << endl;
  }


  map<string,string> label_to_values;
  
  if( !read_config_file_lines( filename, label_to_values ) )
    return -1;

  // *************** system parameters ********************************* 
  ret = parse_single_int_val( label_to_values, "NUMBER_CHANNELS", config->NUMBER_CHANNELS, ignore_missing );//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -2;
  
  ret = parse_single_int_val( label_to_values, "C_CONTROL", config->C_CONTROL, ignore_missing ) ;//no
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -3;

  ret = parse_single_dbl_val( label_to_values, "REQ_RUNTIME", config->REQ_RUNTIME, ignore_missing ) ;//no
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -7;

  ret = parse_single_int_val( label_to_values, "POLL_TIME", config->POLL_TIME, ignore_missing ) ;//no
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -7;
  
  // *************** module parameters ************************************

   // --------------- MODULE_CSRA/B bits -------------------------------------
  if (ignore_missing==0)           // initialize only when reading defaults 
      config->MODULE_CSRA = 0;

  ret = parse_single_bool_val( label_to_values, "MCSRA_CWGROUP_00", bit, ignore_missing ) ;//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -1;
  if(ret==0) config->MODULE_CSRA = SetOrClrBit(0, config->MODULE_CSRA, bit); 
  
  ret = parse_single_bool_val( label_to_values, "MCSRA_FPVETO_05", bit, ignore_missing ) ;//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -1;
  if(ret==0) config->MODULE_CSRA = SetOrClrBit(5, config->MODULE_CSRA, bit); 
  
  ret = parse_single_bool_val( label_to_values, "MCSRA_FPPEDGE_07", bit, ignore_missing ) ;//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -2;
  if(ret==0) config->MODULE_CSRA = SetOrClrBit(7, config->MODULE_CSRA, bit); 
  
  if (ignore_missing==0)           // initialize only when reading defaults 
      config->MODULE_CSRB = 0;
                         
  ret = parse_single_bool_val( label_to_values, "MCSRB_TERM01_01", bit, ignore_missing ) ;//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -3;
  if(ret==0) config->MODULE_CSRB = SetOrClrBit(1, config->MODULE_CSRB, bit);  
 
  ret = parse_single_bool_val( label_to_values, "MCSRB_TERM23_02", bit, ignore_missing ) ;//ok
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -4;
  if(ret==0) config->MODULE_CSRB = SetOrClrBit(2, config->MODULE_CSRB, bit);  
  

//   printf("COINCIDENCE_PATTERN = 0x%x\n",config->COINCIDENCE_PATTERN);
   // --------------- COINC PATTERN bits -------------------------------------
  if (ignore_missing==0)           // initialize only when reading defaults  
       config->COINCIDENCE_PATTERN = 0;

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0000", bit, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -5;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(0, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0001", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -6;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(1, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0010", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -7;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(2, config->COINCIDENCE_PATTERN, bit);  
  
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0011", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -8;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(3, config->COINCIDENCE_PATTERN, bit);  
  
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0100", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )     return -9;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(4, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0101", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -10;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(5, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0110", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -11;  
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(6, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_0111", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -12;  
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(7, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1000", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -13;  
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(8, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1001", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -14;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(9, config->COINCIDENCE_PATTERN, bit);  
                                         
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1010", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -15;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(10, config->COINCIDENCE_PATTERN, bit);  
                                         
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1011", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -16;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(11, config->COINCIDENCE_PATTERN, bit);  
                                         
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1100", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -17;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(12, config->COINCIDENCE_PATTERN, bit);  

  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1101", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -18;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(13, config->COINCIDENCE_PATTERN, bit);  
                                         
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1110", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -19;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(14, config->COINCIDENCE_PATTERN, bit);  
                                         
  ret = parse_single_bool_val( label_to_values, "COINC_PATTERN_1111", bit, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -20;
  if(ret==0) config->COINCIDENCE_PATTERN = SetOrClrBit(15, config->COINCIDENCE_PATTERN, bit);  
                                         
//   printf("COINCIDENCE_PATTERN = 0x%x\n",config->COINCIDENCE_PATTERN);

  // --------------- Other module parameters -------------------------------------
  
  ret = parse_single_dbl_val( label_to_values, "COINCIDENCE_WINDOW", config->COINCIDENCE_WINDOW, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -7;
  
  ret = parse_single_int_val( label_to_values, "RUN_TYPE", config->RUN_TYPE, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -8;

  ret = parse_single_int_val( label_to_values, "FILTER_RANGE", config->FILTER_RANGE, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -9;
  
  ret = parse_single_int_val( label_to_values, "ACCEPT_PATTERN", config->ACCEPT_PATTERN, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -10;
  
  ret = parse_single_int_val( label_to_values, "SYNC_AT_START", config->SYNC_AT_START, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -11;
  
  ret = parse_single_dbl_val( label_to_values, "HV_DAC", config->HV_DAC, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -9;
  
  ret = parse_single_int_val( label_to_values, "SERIAL_IO", config->SERIAL_IO, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -10;
  
  ret = parse_single_int_val( label_to_values, "AUX_CTRL", config->AUX_CTRL, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )   return -11;
  
  //unsigned int MOD_U4, MOD_U3, MOD_U2, MOD_U1, MOD_U0;

  // *************** channel parameters ************************************

     // --------------- Channel CSR bits -------------------------------------
  
   if (ignore_missing==0)           // initialize only when reading defaults 
   {
      for( int i = 0; i < NCHANNELS; ++i )
     {
       config->CHANNEL_CSRA[i] = 0;
       config->CHANNEL_CSRB[i] = 0;
       config->CHANNEL_CSRC[i] = 0;
     }
  }

  ret = parse_multiple_bool_val( label_to_values, "CCSRA_GROUP_00", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -26;
  if(ret==0) 
     for( int i = 0; i < NCHANNELS; ++i )
       config->CHANNEL_CSRA[i] = SetOrClrBit(0, config->CHANNEL_CSRA[i], bits[i]);  

  ret = parse_multiple_bool_val( label_to_values, "CCSRA_GOOD_02", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -27;
  if(ret==0) 
     for( int i = 0; i < NCHANNELS; ++i )
       config->CHANNEL_CSRA[i] = SetOrClrBit(2, config->CHANNEL_CSRA[i], bits[i]);  
                               
  ret = parse_multiple_bool_val( label_to_values, "CCSRA_TRIGENA_04", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -28;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
       config->CHANNEL_CSRA[i] = SetOrClrBit(4, config->CHANNEL_CSRA[i], bits[i]);  
                               
  ret = parse_multiple_bool_val( label_to_values, "CCSRA_INVERT_05", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -29;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
       config->CHANNEL_CSRA[i] = SetOrClrBit(5, config->CHANNEL_CSRA[i], bits[i]);  
                               
  ret = parse_multiple_bool_val( label_to_values, "CCSRA_VETO_REJLO_06", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -30;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRA[i] = SetOrClrBit(6, config->CHANNEL_CSRA[i], bits[i]);  
                              
  ret = parse_multiple_bool_val( label_to_values, "CCSRA_NEGE_09", bits, ignore_missing) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -31;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(9, config->CHANNEL_CSRA[i], bits[i]);  
                            
  ret = parse_multiple_bool_val( label_to_values, "CCSRA_GATE_REJLO_12", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -32;
  if(ret==0) 
   for( int i = 0; i < NCHANNELS; ++i )
     config->CHANNEL_CSRA[i] = SetOrClrBit(12, config->CHANNEL_CSRA[i], bits[i]);  
 
  //   for( int i = 0; i < NCHANNELS; ++i )
  //     printf("CHANNEL_CSRA = 0x%x\n",config->CHANNEL_CSRA[i]);                       

  //CSRC
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_VETO_REJHI_00", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -33;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(0, config->CHANNEL_CSRC[i], bits[i]);  
 
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_REJHI_01", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -34;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(1, config->CHANNEL_CSRC[i], bits[i]);  

  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_FROMVETO_02", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -35;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(2, config->CHANNEL_CSRC[i], bits[i]);   

  ret = parse_multiple_bool_val( label_to_values, "CCSRC_PILEUP_DISABLE_03", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -2;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(3, config->CHANNEL_CSRC[i], bits[i]);   

  ret = parse_multiple_bool_val( label_to_values, "CCSRC_RBAD_DISABLE_04", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -36;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(4, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_PILEUP_INVERT_05", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -37;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(5, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_PILEUP_PAUSE_06", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -38;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(6, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_FEDGE_07", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -39;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(7, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_STATS_08", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -40;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(8, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_VETO_FEDGE_09", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -41;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(9, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_ISPULSE_10", bits, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -42;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(10, config->CHANNEL_CSRC[i], bits[i]);   
       
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_CPC2PSA_14", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -43;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(14, config->CHANNEL_CSRC[i], bits[i]);   
    
  ret = parse_multiple_bool_val( label_to_values, "CCSRC_GATE_PULSEFEDGE_15", bits, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )    return -44;
  if(ret==0) 
    for( int i = 0; i < NCHANNELS; ++i )
      config->CHANNEL_CSRC[i] = SetOrClrBit(15, config->CHANNEL_CSRC[i], bits[i]);   
  

      // --------------- other channel parameters -------------------------------------
  ret = parse_multiple_dbl_val( label_to_values, "ENERGY_RISETIME", config->ENERGY_RISETIME, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -15;
  
  ret = parse_multiple_dbl_val( label_to_values, "ENERGY_FLATTOP", config->ENERGY_FLATTOP, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -16;
  
  ret = parse_multiple_dbl_val( label_to_values, "TRIGGER_RISETIME", config->TRIGGER_RISETIME, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -17;
  
  ret = parse_multiple_dbl_val( label_to_values, "TRIGGER_FLATTOP", config->TRIGGER_FLATTOP, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -18;
  
  ret = parse_multiple_dbl_val( label_to_values, "TRIGGER_THRESHOLD", config->TRIGGER_THRESHOLD, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -19;
  
  ret = parse_multiple_dbl_val( label_to_values, "ANALOG_GAIN", config->ANALOG_GAIN, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -20;
  
  ret = parse_multiple_dbl_val( label_to_values, "DIG_GAIN", config->DIG_GAIN, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -21;
  
  ret = parse_multiple_dbl_val( label_to_values, "VOFFSET", config->VOFFSET, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -22;
  
  ret = parse_multiple_dbl_val( label_to_values, "TRACE_LENGTH", config->TRACE_LENGTH, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -23;
  
  ret = parse_multiple_dbl_val( label_to_values, "TRACE_DELAY", config->TRACE_DELAY, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -24;
  
  ret = parse_multiple_int_val( label_to_values, "BINFACTOR", config->BINFACTOR, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -27;
  
  ret = parse_multiple_dbl_val( label_to_values, "TAU", config->TAU, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -28;
  
  ret = parse_multiple_int_val( label_to_values, "BLCUT", config->BLCUT, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -29;
  
  ret = parse_multiple_dbl_val( label_to_values, "XDT", config->XDT, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -30;
  
  ret = parse_multiple_dbl_val( label_to_values, "BASELINE_PERCENT", config->BASELINE_PERCENT, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -31;
  
  ret = parse_multiple_int_val( label_to_values, "PSA_THRESHOLD", config->PSA_THRESHOLD, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -32;
  
  ret = parse_multiple_int_val( label_to_values, "INTEGRATOR", config->INTEGRATOR, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -33;
  
  ret = parse_multiple_dbl_val( label_to_values, "GATE_WINDOW", config->GATE_WINDOW, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -34;
  
  ret = parse_multiple_dbl_val( label_to_values, "GATE_DELAY", config->GATE_DELAY, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -35;
  
  ret = parse_multiple_dbl_val( label_to_values, "COINC_DELAY", config->COINC_DELAY, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -36;
  
  ret = parse_multiple_int_val( label_to_values, "BLAVG", config->BLAVG, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -37;
  
  ret = parse_multiple_int_val( label_to_values, "QDC0_LENGTH", config->QDC0_LENGTH, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -38;
  
  ret = parse_multiple_int_val( label_to_values, "QDC1_LENGTH", config->QDC1_LENGTH, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -39;
  
  ret = parse_multiple_int_val( label_to_values, "QDC0_DELAY", config->QDC0_DELAY, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -40;
  
  ret = parse_multiple_int_val( label_to_values, "QDC1_DELAY", config->QDC1_DELAY, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -41;

  ret = parse_multiple_int_val( label_to_values, "QDC_DIV8", config->QDC_DIV8, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -42;
    
  ret = parse_multiple_dbl_val( label_to_values, "MCA2D_SCALEX", config->MCA2D_SCALEX, ignore_missing ) ;
  if( (ignore_missing==0 && ret==1) || (ret<0) ) return -43;

  ret = parse_multiple_dbl_val( label_to_values, "MCA2D_SCALEY", config->MCA2D_SCALEY, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -44;

  ret = parse_multiple_dbl_val( label_to_values, "PSA_NG_THRESHOLD", config->PSA_NG_THRESHOLD, ignore_missing );
  if( (ignore_missing==0 && ret==1) || (ret<0) )  return -45;
 
 
    if(ignore_missing==1) 
  {
      cerr << endl;
  }

  return 0;
}//init_PixieNetFippiConfig_from_file(...)



int PKU_init_PixieNetFippiConfig_from_file(const char * const filename, struct PixieNetFippiConfig *config)
{
  int set;
  vector<int>* setsint = new vector<int>;
  vector<double>* setsdouble = new vector<double>;
  int retn;

  config->NUMBER_CHANNELS = wuReadData::ReadValue<unsigned int>("NUMBER_CHANNELS",std::string(filename));
  // std::cout<<"NUMBER_CHANNELS  "<<config->NUMBER_CHANNELS<<std::endl;

  config->SYNC_AT_START = wuReadData::ReadValue<unsigned int>("SYNC_AT_START",std::string(filename));
  // std::cout<<"SYNC_AT_START  "<<config->SYNC_AT_START<<std::endl;

  config->AUX_CTRL = wuReadData::ReadValue<unsigned int>("AUX_CTRL",std::string(filename));
  // std::cout<<"AUX_CTRL  "<<config->AUX_CTRL<<std::endl;

  config->SERIAL_IO = wuReadData::ReadValue<unsigned int>("SERIAL_IO",std::string(filename));
  // std::cout<<"SERIAL_IO  "<<config->SERIAL_IO<<std::endl;

  config->COINCIDENCE_WINDOW = wuReadData::ReadValue<double>("COINCIDENCE_WINDOW",std::string(filename));
  // std::cout<<"COINCIDENCE_WINDOW  "<<config->COINCIDENCE_WINDOW<<std::endl;

  config->HV_DAC = wuReadData::ReadValue<double>("HV_DAC",std::string(filename));
  // std::cout<<"HV_DAC  "<<config->HV_DAC<<std::endl;

  config->FILTER_RANGE = wuReadData::ReadValue<unsigned int>("FILTER_RANGE",std::string(filename));
  // std::cout<<"FILTER_RANGE  "<<config->FILTER_RANGE<<std::endl;


  config->MODULE_CSRA = 0;
  set = wuReadData::ReadValue<int>("MCSRA_CWGROUP_00",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(0, config->MODULE_CSRA, set); 
  set = wuReadData::ReadValue<int>("MCSRA_FPVETO_05",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(5, config->MODULE_CSRA, set); 
  set = wuReadData::ReadValue<int>("MCSRA_FPPEDGE_07",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(7, config->MODULE_CSRA, set); 
  // std::cout<<"MODULE_CSRA  "<<hex<<config->MODULE_CSRA<<std::endl;

  config->MODULE_CSRB = 0;
  set = wuReadData::ReadValue<int>("MCSRB_TERM01_01",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(1, config->MODULE_CSRB, set); 
  set = wuReadData::ReadValue<int>("MCSRB_TERM23_02",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(2, config->MODULE_CSRB, set); 
  set = wuReadData::ReadValue<int>("MCSRB_PDCH0_04",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(4, config->MODULE_CSRB, set); 
  set = wuReadData::ReadValue<int>("MCSRB_PDCH1_05",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(5, config->MODULE_CSRB, set); 
  set = wuReadData::ReadValue<int>("MCSRB_PDCH2_06",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(6, config->MODULE_CSRB, set); 
  set = wuReadData::ReadValue<int>("MCSRB_PDCH3_07",std::string(filename));
  config->MODULE_CSRB = SetOrClrBit(7, config->MODULE_CSRB, set); 
  // std::cout<<"MODULE_CSRB  "<<hex<<config->MODULE_CSRB<<std::endl;


  config->COINCIDENCE_PATTERN = 0;
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0000",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(0, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0001",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(1, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0010",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(2, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0011",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(3, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0100",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(4, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0101",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(5, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0110",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(6, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_0111",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(7, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1000",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(8, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1001",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(9, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1010",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(10, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1011",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(11, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1100",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(12, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1101",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(13, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1110",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(14, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_1111",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(15, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_bit16",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(16, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_bit17",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(17, config->COINCIDENCE_PATTERN, set); 
  set = wuReadData::ReadValue<int>("COINC_PATTERN_bit18",std::string(filename));
  config->COINCIDENCE_PATTERN = SetOrClrBit(18, config->COINCIDENCE_PATTERN, set); 
  // std::cout<<"COINCIDENCE_PATTERN  "<<hex<<config->COINCIDENCE_PATTERN<<std::endl;




  for( int i = 0; i < NCHANNELS; ++i )
    {
      config->CHANNEL_CSRA[i] = 0;
      config->CHANNEL_CSRB[i] = 0;
      config->CHANNEL_CSRC[i] = 0;
    }

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_GROUP_00",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(0, config->CHANNEL_CSRA[i], setsint->at(i)); 
  
  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_GOOD_02",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(2, config->CHANNEL_CSRA[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_TRIGENA_04",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(4, config->CHANNEL_CSRA[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_INVERT_05",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(5, config->CHANNEL_CSRA[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_VETO_REJLO_06",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(6, config->CHANNEL_CSRA[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_NEGE_09",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(9, config->CHANNEL_CSRA[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRA_GATE_REJLO_12",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRA[i] = SetOrClrBit(12, config->CHANNEL_CSRA[i], setsint->at(i)); 


  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_VETO_REJHI_00",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(0, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_REJHI_01",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(1, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_FROMVETO_02",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(2, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_PILEUP_DISABLE_03",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(3, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_RBAD_DISABLE_04",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(4, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_PILEUP_INVERT_05",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(5, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_PILEUP_PAUSE_06",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(6, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_FEDGE_07",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(7, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_STATS_08",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(8, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_VETO_FEDGE_09",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(9, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_ISPULSE_10",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(10, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_CPC2PSA_14",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(14, config->CHANNEL_CSRC[i], setsint->at(i)); 

  setsint->clear();
  retn = wuReadData::ReadVector("CCSRC_GATE_PULSEFEDGE_15",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i )
    config->CHANNEL_CSRC[i] = SetOrClrBit(15, config->CHANNEL_CSRC[i], setsint->at(i)); 


  setsdouble->clear();
  retn = wuReadData::ReadVector("ENERGY_RISETIME",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ENERGY_RISETIME[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("ENERGY_FLATTOP",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ENERGY_FLATTOP[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRIGGER_RISETIME",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRIGGER_RISETIME[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRIGGER_FLATTOP",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRIGGER_FLATTOP[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRIGGER_THRESHOLD",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRIGGER_THRESHOLD[i] = setsdouble->at(i);


  setsdouble->clear();
  retn = wuReadData::ReadVector("ANALOG_GAIN",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ANALOG_GAIN[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("VOFFSET",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->VOFFSET[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRACE_LENGTH",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRACE_LENGTH[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRACE_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRACE_DELAY[i] = setsdouble->at(i);

  setsint->clear();
  retn = wuReadData::ReadVector("PSA_THRESHOLD",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->PSA_THRESHOLD[i] = (unsigned int)setsint->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("GATE_WINDOW",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->GATE_WINDOW[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("GATE_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->GATE_DELAY[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("COINC_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->COINC_DELAY[i] = setsdouble->at(i);


  setsint->clear();
  retn = wuReadData::ReadVector("QDC0_LENGTH",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->QDC0_LENGTH[i] = (unsigned int)setsint->at(i);

  setsint->clear();
  retn = wuReadData::ReadVector("QDC1_LENGTH",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->QDC1_LENGTH[i] = (unsigned int)setsint->at(i);

  setsint->clear();
  retn = wuReadData::ReadVector("QDC0_DELAY",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->QDC0_DELAY[i] = (unsigned int)setsint->at(i);

  setsint->clear();
  retn = wuReadData::ReadVector("QDC1_DELAY",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->QDC1_DELAY[i] = (unsigned int)setsint->at(i);

  setsint->clear();
  retn = wuReadData::ReadVector("QDC_DIV8",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->QDC_DIV8[i] = (unsigned int)setsint->at(i);



  if(setsint) delete setsint;
  if(setsdouble) delete setsdouble;

  // config-> = wuReadData::ReadValue<int>("",std::string(filename));
  // std::cout<<"  "<<config-><<std::endl;

 // set = wuReadData::ReadValue<int>("",std::string(filename));
  return 0;
}


void PrintInterface() 
{
  printf("\n  [q]   Quit\n");
  printf("  [s]   Start/Stop acquisition\n");
  printf("  [t]   Send a software trigger\n");
  printf("  [w]   Enable/Disable continuous writing to output file\n");
  printf("  [R]   Reload board parameters file and restart\n");
  printf("  [p]   Enable/Disable  plot mode\n");
  // printf("  [0]   Plot recently single on plot mode \n");
  // printf("  [2/8] Minus/Plus one channel on plot mode\n");
  // printf("  [4/6] Minus/Plus one board on plot mode\n");
  printf("--------------------------------------------------------------------------\n");

}


void RunManagerInit(DigitizerRun_t *RunManager)
{
  RunManager->RunNumber = -1;
  RunManager->FileNo = -1;
  
  RunManager->Quit = false;
  RunManager->AcqRun = false;
  // RunManager->Nb = 0;

  RunManager->WriteFlag = false;
  
  // memset(RunManager->PrevTime, 0, MAXNB*MaxNChannels*sizeof(uint64_t));


  // std::string PathToRawData = ReadValue<std::string>("PathToRawData",PKU_DGTZ_GlobalParametersFileName);
  // sprintf(RunManager->PathToRawData,"%s",PathToRawData.c_str());
  // std::cout<<RunManager->PathToRawData<<std::endl;

  // RunManager->PlotFlag = false;
  // RunManager->DoPlotBoard = 0;
  // RunManager->DoPlotChannel = 0;
  // RunManager->PlotChooseN = ReadValue<int>("PlotChooseN",PKU_DGTZ_GlobalParametersFileName);
}

void CheckKeyboard(DigitizerRun_t *PKU_DGTZ_RunManager)
{
  int b;

  if(kbhit())
    {
      PKU_DGTZ_RunManager->Key = getch();
      std::cout<<PKU_DGTZ_RunManager->Key<<std::endl;
      switch(PKU_DGTZ_RunManager->Key)
	{
	case 'q' :
	  {
	    if(PKU_DGTZ_RunManager->AcqRun) 
	      {
		printf("Please enter [s] to stop and enter [q] to quit.\n");
		break;
	      }
	    PKU_DGTZ_RunManager->Quit = true;
	    break;
	  }


	case '\n' :
	  PrintInterface();
	  break;

	default:
	  break;
	}

    }

}

static struct termios g_old_kbd_mode;
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
static void cooked(void)
{
  tcsetattr(0, TCSANOW, &g_old_kbd_mode);
}


static void raw(void)
{
  static char init;
  struct termios new_kbd_mode;

  if(init)
    return;
  /* put keyboard (stdin, actually) in raw, unbuffered mode */
  tcgetattr(0, &g_old_kbd_mode);
  memcpy(&new_kbd_mode, &g_old_kbd_mode, sizeof(struct termios));
  new_kbd_mode.c_lflag &= ~(ICANON | ECHO);
  new_kbd_mode.c_cc[VTIME] = 0;
  new_kbd_mode.c_cc[VMIN] = 1;
  tcsetattr(0, TCSANOW, &new_kbd_mode);
  /* when we exit, go back to normal, "cooked" mode */
  atexit(cooked);

  init = 1;
}

int getch(void)
{
  unsigned char temp;

  raw();
  /* stdin = fd 0 */
  if(read(0, &temp, 1) != 1)
    return 0;
  return temp;

}

int kbhit()
{
  struct timeval timeout;
  fd_set read_handles;
  int status;

  raw();
  /* check stdin (fd 0) for activity */
  FD_ZERO(&read_handles);
  FD_SET(0, &read_handles);
  timeout.tv_sec = timeout.tv_usec = 0;
  status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
  if(status < 0)
    {
      printf("select() failed in kbhit()\n");
      exit(1);
    }
  return (status);
}

void Sleep(int t) 
{
  usleep( t*1000 );
}

void DoInTerminal(char *terminal)
{
  system(terminal);
}

long get_time()
{
  long time_ms;
  struct timeval t1;
  struct timezone tz;
  gettimeofday(&t1, &tz);
  time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
  return time_ms;
}
