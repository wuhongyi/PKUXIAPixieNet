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
#include <math.h>
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
// #include "PixieNetCommon.h"
#ifdef __cplusplus
extern "C"
#endif
{
#include "PixieNetCommon.h"
#ifdef __cplusplus
}
#endif

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



int PKU_init_PixieNetFippiConfig_from_file(const char * const filename, struct PixieNetFippiConfig *config,struct  DigitizerFPGAUnit *fpgapar)
{
  int set;
  vector<int>* setsint = new vector<int>;
  vector<double>* setsdouble = new vector<double>;
  int retn;

  config->NUMBER_CHANNELS = wuReadData::ReadValue<unsigned int>("NUMBER_CHANNELS",std::string(filename));
  // std::cout<<"NUMBER_CHANNELS  "<<config->NUMBER_CHANNELS<<std::endl;
  if(config->NUMBER_CHANNELS != NCHANNELS) 
    {
      printf("Invalid NUMBER_CHANNELS = %d, should be %d\n",config->NUMBER_CHANNELS,NCHANNELS);
      return -1;
    }

  config->SYNC_AT_START = wuReadData::ReadValue<unsigned int>("SYNC_AT_START",std::string(filename));
  // std::cout<<"SYNC_AT_START  "<<config->SYNC_AT_START<<std::endl;
  if(config->SYNC_AT_START > 1) 
    {
      printf("Invalid SYNC_AT_START = %d, can only be 0 and 1\n",config->SYNC_AT_START);
      return -1;
    }


  config->AUX_CTRL = wuReadData::ReadValue<unsigned int>("AUX_CTRL",std::string(filename));
  // std::cout<<"AUX_CTRL  "<<config->AUX_CTRL<<std::endl;
  if(config->AUX_CTRL > 65535) 
    {
      printf("Invalid AUX_CTRL = 0x%x\n",config->AUX_CTRL);
      return -1;
    }


  config->SERIAL_IO = wuReadData::ReadValue<unsigned int>("SERIAL_IO",std::string(filename));
  // std::cout<<"SERIAL_IO  "<<config->SERIAL_IO<<std::endl;
  if(config->SERIAL_IO > 65535) 
    {
      printf("Invalid SERIAL_IO = 0x%x\n",config->SERIAL_IO);
      return -1;
    }


  config->COINCIDENCE_WINDOW = wuReadData::ReadValue<double>("COINCIDENCE_WINDOW",std::string(filename));
  // std::cout<<"COINCIDENCE_WINDOW  "<<config->COINCIDENCE_WINDOW<<std::endl;
  fpgapar->CW = (int)floorf(config->COINCIDENCE_WINDOW*SYSTEM_CLOCK_MHZ);       // multiply time in us *  # ticks per us = time in ticks
  if( (fpgapar->CW > MAX_CW) | (fpgapar->CW < MIN_CW) ) 
    {
      printf("Invalid COINCIDENCE_WINDOW = %f, must be between %f and %f us\n",config->COINCIDENCE_WINDOW, (double)MIN_CW/SYSTEM_CLOCK_MHZ, (double)MAX_CW/SYSTEM_CLOCK_MHZ);
      return -1;
    }


  config->HV_DAC = wuReadData::ReadValue<double>("HV_DAC",std::string(filename));
  // std::cout<<"HV_DAC  "<<config->HV_DAC<<std::endl;
  int mval = (int)floor((config->HV_DAC/5.0) * 65535);		// map 0..5V range to 0..64K	
  if(mval > 65535) 
    {
      printf("Invalid HV_DAC = %f, can only be between 0 and 5V\n",config->HV_DAC);
      return -1;
    }


  config->FILTER_RANGE = wuReadData::ReadValue<unsigned int>("FILTER_RANGE",std::string(filename));
  // std::cout<<"FILTER_RANGE  "<<config->FILTER_RANGE<<std::endl;
  fpgapar->FR = config->FILTER_RANGE;
  if( (fpgapar->FR > MAX_FR) | (fpgapar->FR < MIN_FR) )
    {
      printf("Invalid FILTER_RANGE = %d, must be between %d and %d\n",fpgapar->FR,MIN_FR, MAX_FR);
      return -1;
    }

  config->MODULE_CSRA = 0;
  set = wuReadData::ReadValue<int>("MCSRA_CWGROUP_00",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(0, config->MODULE_CSRA, set); 
  set = wuReadData::ReadValue<int>("MCSRA_FPVETO_05",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(5, config->MODULE_CSRA, set); 
  set = wuReadData::ReadValue<int>("MCSRA_FPPEDGE_07",std::string(filename));
  config->MODULE_CSRA = SetOrClrBit(7, config->MODULE_CSRA, set); 
  // std::cout<<"MODULE_CSRA  "<<hex<<config->MODULE_CSRA<<std::endl;
  if(config->MODULE_CSRA > 65535) 
    {
      printf("Invalid MODULE_CSRA = 0x%x\n",config->MODULE_CSRA);
      return -1;
    }


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
  if(config->MODULE_CSRB > 65535)
    {
      printf("Invalid MODULE_CSRB = 0x%x\n",config->MODULE_CSRB);
      return -1;
    }


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
  if(config->COINCIDENCE_PATTERN > 65535)
    {
      printf("Invalid COINCIDENCE_PATTERN = 0x%x\n",config->COINCIDENCE_PATTERN);
      return -1;
    }
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

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      if(config->CHANNEL_CSRA[k] > 65535) 
	{
	  printf("Invalid CHANNEL_CSRA = 0x%x\n",config->CHANNEL_CSRA[k]);
	  return -1;
	} 
      if(config->CHANNEL_CSRC[k] > 65535) 
	{
	  printf("Invalid CHANNEL_CSRC = 0x%x\n",config->CHANNEL_CSRC[k]);
	  return -1;
	}  
    }


  setsdouble->clear();
  retn = wuReadData::ReadVector("ENERGY_RISETIME",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ENERGY_RISETIME[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("ENERGY_FLATTOP",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ENERGY_FLATTOP[i] = setsdouble->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      fpgapar->SL[k] = (int)floorf(config->ENERGY_RISETIME[k] * FILTER_CLOCK_MHZ);
      fpgapar->SL[k] = fpgapar->SL[k] >> fpgapar->FR;
      if(fpgapar->SL[k] < MIN_SL) 
	{
	  printf("Invalid ENERGY_RISETIME = %f, minimum %f us at this filter range\n",config->ENERGY_RISETIME[k],(double)((MIN_SL<<fpgapar->FR)/FILTER_CLOCK_MHZ));
	  return -1;
	} 
      fpgapar->SG[k] = (int)floorf(config->ENERGY_FLATTOP[k] * FILTER_CLOCK_MHZ);
      fpgapar->SG[k] = fpgapar->SG[k] >> fpgapar->FR;
      if(fpgapar->SG[k] < MIN_SG) 
	{
	  printf("Invalid ENERGY_FLATTOP = %f, minimum %f us at this filter range\n",config->ENERGY_FLATTOP[k],(double)((MIN_SG<<fpgapar->FR)/FILTER_CLOCK_MHZ));
	  return -1;
	} 
      if( (fpgapar->SL[k]+fpgapar->SG[k]) > MAX_SLSG) 
	{
	  printf("Invalid combined energy filter, maximum %f us at this filter range\n",(double)((MAX_SLSG<<fpgapar->FR)/FILTER_CLOCK_MHZ));
	  return -1;
	} 
    }



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

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      fpgapar->FL[k] = (int)floorf(config->TRIGGER_RISETIME[k] * FILTER_CLOCK_MHZ);
      if(fpgapar->FL[k] < MIN_FL) 
	{
	  printf("Invalid TRIGGER_RISETIME = %f, minimum %f us\n",config->TRIGGER_RISETIME[k],(double)(MIN_FL/FILTER_CLOCK_MHZ));
	  return -1;
	} 
      fpgapar->FG[k] = (int)floorf(config->TRIGGER_FLATTOP[k] * FILTER_CLOCK_MHZ);
      if(fpgapar->FG[k] < MIN_FL) 
	{
	  printf("Invalid TRIGGER_FLATTOP = %f, minimum %f us\n",config->TRIGGER_FLATTOP[k],(double)(MIN_FG/FILTER_CLOCK_MHZ));
	  return -1;
	} 
      if( (fpgapar->FL[k]+fpgapar->FG[k]) > MAX_FLFG) 
	{
	  printf("Invalid combined trigger filter, maximum %f us\n",(double)(MAX_FLFG/FILTER_CLOCK_MHZ));
	  return -1;
	} 
      fpgapar->TH[k] = (int)floor(config->TRIGGER_THRESHOLD[k]*fpgapar->FL[k]/8.0);
      if(fpgapar->TH[k] > MAX_TH)     
	{
	  printf("Invalid TRIGGER_THRESHOLD = %f, maximum %f at this trigger filter rise time\n",config->TRIGGER_THRESHOLD[k],MAX_TH*8.0/(double)fpgapar->FL[k]);
	  return -1;
	} 
    }



  setsdouble->clear();
  retn = wuReadData::ReadVector("ANALOG_GAIN",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->ANALOG_GAIN[i] = setsdouble->at(i);

  // current version only has 2 gains: 2 and 5. applied via I2C below, only save bit pattern here
  for(int k = 0; k < NCHANNELS; k ++ )
    {
      if( !( (config->ANALOG_GAIN[k] == GAIN_HIGH)  || (config->ANALOG_GAIN[k] == GAIN_LOW)   ) ) 
	{
	  printf("ANALOG_GAIN = %f not matching available gains exactly, rounding to nearest\n",config->ANALOG_GAIN[k]);
	}
    }

  setsdouble->clear();
  retn = wuReadData::ReadVector("VOFFSET",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->VOFFSET[i] = setsdouble->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      int dac = (int)floor((1 - config->VOFFSET[k]/ V_OFFSET_MAX) * 32768);	
      if(dac > 65535)  
	{
	  printf("Invalid VOFFSET = %f, must be between %f and -%f\n",config->VOFFSET[k], V_OFFSET_MAX-0.05, V_OFFSET_MAX-0.05);
	  return -1;
	}
    }


  setsdouble->clear();
  retn = wuReadData::ReadVector("TRACE_LENGTH",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRACE_LENGTH[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("TRACE_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->TRACE_DELAY[i] = setsdouble->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      fpgapar->TL[k] = BLOCKSIZE_400*(int)floor(config->TRACE_LENGTH[k]*ADC_CLK_MHZ/BLOCKSIZE_400);       // multiply time in us *  # ticks per us = time in ticks; must be multiple of BLOCKSIZE_400
      if(fpgapar->TL[k] > MAX_TL)  
	{
	  printf("Invalid TRACE_LENGTH = %f, maximum %f us\n",config->TRACE_LENGTH[k],(double)MAX_TL/ADC_CLK_MHZ);
	  return -1;
	}
      if(fpgapar->TL[k] < config->TRACE_LENGTH[k]*ADC_CLK_MHZ)  
	{
	  printf("TRACE_LENGTH[%d] will be rounded off to = %f us, %d samples\n",k,(double)fpgapar->TL[k]/ADC_CLK_MHZ,fpgapar->TL[k]);
	}
      fpgapar->TD[k] = (int)floor(config->TRACE_DELAY[k]*ADC_CLK_MHZ);       // multiply time in us *  # ticks per us = time in ticks
      if(fpgapar->TD[k] > MAX_TL-TWEAK_UD)  
	{
	  printf("Invalid TRACE_DELAY = %f, maximum %f us\n",config->TRACE_DELAY[k],(double)(MAX_TL-TWEAK_UD)/ADC_CLK_MHZ);
	  return -1;
	}
    }


  setsint->clear();
  retn = wuReadData::ReadVector("PSA_THRESHOLD",std::string(filename),setsint);
  // std::cout<<setsint->at(0)<<"  "<<setsint->at(1)<<"  "<<setsint->at(2)<<"  "<<setsint->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->PSA_THRESHOLD[i] = (unsigned int)setsint->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      if(config->PSA_THRESHOLD[k] > MAX_PSATH) 
	{
	  printf("Invalid PSA_THRESHOLD = %d, maximum %d\n",config->PSA_THRESHOLD[k],MAX_PSATH);
	  return -1;                                                       
	}
    }



  setsdouble->clear();
  retn = wuReadData::ReadVector("GATE_WINDOW",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->GATE_WINDOW[i] = setsdouble->at(i);

  setsdouble->clear();
  retn = wuReadData::ReadVector("GATE_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->GATE_DELAY[i] = setsdouble->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      fpgapar->GW[k] = (int)floor(config->GATE_WINDOW[k] * FILTER_CLOCK_MHZ);
      if(fpgapar->GW[k] > MAX_GW)
	{
	  printf("Invalid GATE_WINDOW = %f, maximum %d us\n",config->GATE_WINDOW[k],MAX_GW/FILTER_CLOCK_MHZ);
	  return -1;
	}
      fpgapar->GD[k] = (int)floor(config->GATE_DELAY[k]*FILTER_CLOCK_MHZ);
      if(fpgapar->GD[k] > MAX_GD) 
	{
	  printf("Invalid GATE_DELAY = %f, maximum %d us\n",config->GATE_DELAY[k],MAX_GD/FILTER_CLOCK_MHZ);
	  return -1;
	}
    }


  setsdouble->clear();
  retn = wuReadData::ReadVector("COINC_DELAY",std::string(filename),setsdouble);
  // std::cout<<setsdouble->at(0)<<"  "<<setsdouble->at(1)<<"  "<<setsdouble->at(2)<<"  "<<setsdouble->at(3)<<std::endl;
  for( int i = 0; i < NCHANNELS; ++i ) config->COINC_DELAY[i] = setsdouble->at(i);

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      mval = (int)floor(config->COINC_DELAY[k] * ADC_CLK_MHZ);    
      if(mval > MAX_CD) 
	{
	  printf("Invalid COINC_DELAY = %f, maximum %d us\n",config->COINC_DELAY[k],MAX_CD/ADC_CLK_MHZ);
	  return -1;
	}
    }


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

  for(int k = 0; k < NCHANNELS; k ++ )
    { 
      if(config->QDC0_LENGTH[k] > MAX_QDCL)    
	{
	  printf("Invalid QDC0_LENGTH = %d, maximum %d samples \n",config->QDC0_LENGTH[k],MAX_QDCL);
	  return -1;
	} 
      if(config->QDC0_LENGTH[k]+config->QDC0_DELAY[k] > MAX_QDCLD)    
	{
	  printf("Invalid QDC0_DELAY = %d, maximum length plus delay %d samples \n",config->QDC0_DELAY[k],MAX_QDCLD);
	  return -1;
	} 
      if(config->QDC1_LENGTH[k] > MAX_QDCL)    
	{
	  printf("Invalid QDC1_LENGTH = %d, maximum %d samples \n",config->QDC1_LENGTH[k],MAX_QDCL);
	  return -1;
	} 
      if(config->QDC1_LENGTH[k]+config->QDC1_DELAY[k] > MAX_QDCLD)    
	{
	  printf("Invalid QDC1_DELAY = %d, maximum length plus delay %d samples \n",config->QDC1_DELAY[k],MAX_QDCLD);
	  return -1;
	} 
      if(config->QDC0_LENGTH[k]+config->QDC0_DELAY[k] > config->QDC1_LENGTH[k]+config->QDC1_DELAY[k])   
	{
	  printf("Invalid QDC1_DELAY/_LENGTH; must finish later than QDC0 \n");
	  return -1;
	} 
    }


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
  printf("  [g]   Read 8K samples from ADC register, visible to web \n");
  printf("  [p]   Enable/Disable  plot mode\n");
  printf("  [4/6] Minus/Plus one channel on plot mode\n");
  printf("  [0]   Plot recently waveform on plot mode, visible to web \n");
  printf("  [f]   Print running status\n");

  // printf("  [t]   Send a software trigger\n");
  // printf("  [w]   Enable/Disable continuous writing to output file\n");
  // printf("  [R]   Reload board parameters file and restart\n");


  

  // printf("  [4/6] Minus/Plus one board on plot mode\n");
  printf("--------------------------------------------------------------------------\n");

}


void RunManagerInit(struct DigitizerRun_t *RunManager)
{
  RunManager->RunNumber = -1;
  RunManager->FileNo = -1;
  
  RunManager->Quit = false;
  RunManager->AcqRun = false;
  // RunManager->Nb = 0;

  RunManager->WriteFlag = false;
  RunManager->PlotFlag = false;  
  RunManager->DoPlotChannel = 0;
  RunManager->PlotRecent = false;  

  // memset(RunManager->PrevTime, 0, MAXNB*MaxNChannels*sizeof(uint64_t));
  // std::string PathToRawData = ReadValue<std::string>("PathToRawData",PKU_DGTZ_GlobalParametersFileName);
  // sprintf(RunManager->PathToRawData,"%s",PathToRawData.c_str());
  // std::cout<<RunManager->PathToRawData<<std::endl;
  // RunManager->PlotChooseN = ReadValue<int>("PlotChooseN",PKU_DGTZ_GlobalParametersFileName);
}

void WriteOneOnlineWaveform(int ch,int point,uint16_t *waveform)
{
  FILE * fil;
  fil = fopen("online.csv","w");

  fprintf(fil,"sample,ch%d\n",ch);

  //  write to file
  for(int k = 0; k < point; k ++ )
  {
    fprintf(fil,"%d,%d\n ",k,waveform[k]);
  }

 // clean up  
 fclose(fil);
}

void PrintRunningStatus(struct DigitizerRun_t *PKU_DGTZ_RunManager)
{
  PrintInterface();
  printf("Status:\n");
  if(PKU_DGTZ_RunManager->AcqRun) 
    {
      printf("Start acquisition,");
      if(PKU_DGTZ_RunManager->WriteFlag) printf(" Writing file Number: %d\n",PKU_DGTZ_RunManager->RunNumber);
      else printf(" Not Write ......\n");
    }
  else printf("You can enter [s] to start acquisition,enter [g] to read 8K samples from ADC register,...\n");
  if(PKU_DGTZ_RunManager->PlotFlag)
    {
      printf("Monitor: Ch-%d\n",PKU_DGTZ_RunManager->DoPlotChannel);
      if(PKU_DGTZ_RunManager->PlotRecent) printf("Waitting recently waveform !!!\n");
    }
}

void CheckKeyboard(struct DigitizerRun_t *PKU_DGTZ_RunManager,volatile unsigned int *mapped,struct PixieNetFippiConfig *config)
{
  int b;

  if(kbhit())
    {
      PKU_DGTZ_RunManager->Key = getch();
      std::cout<<PKU_DGTZ_RunManager->Key<<std::endl;
      switch(PKU_DGTZ_RunManager->Key)
	{
	case 'f':
	  {
	    PrintRunningStatus(PKU_DGTZ_RunManager);
	    break;
	  }

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

	case 's':
	  {
	    if(PKU_DGTZ_RunManager->AcqRun)
	      {//running,do stop
		// ********************** Run Stop **********************
		// clear RunEnable bit to stop run
		mapped[ACSRIN] = 0;               
		// todo: there may be events left in the buffers. need to stop, then keep reading until nothing left
                    
		mapped[AOUTBLOCK] = OB_RSREG;
		read_print_runstats(0, 0, mapped);// print (small) set of RS to file, visible to web
		mapped[AOUTBLOCK] = OB_IOREG;

		PKU_DGTZ_RunManager->AcqRun = false;
	      }
	    else//stop,do run
	      {
		// ********************** Run Start **********************
		// assign to local variables, including any rounding/discretization
		if(config->SYNC_AT_START) mapped[ARTC_CLR] = 1;              // write to reset time counter
		mapped[AOUTBLOCK] = 2;

		unsigned int startTS =mapped[AREALTIME];
		std::cout<<"StartTime: "<<startTS<<std::endl;

		mapped[ADSP_CLR] = 1;             // write to reset DAQ buffers
		mapped[ACOUNTER_CLR] = 1;         // write to reset RS counters
		mapped[ACSRIN] = 1;               // set RunEnable bit to start run
		mapped[AOUTBLOCK] = OB_EVREG;     // read from event registers

		PKU_DGTZ_RunManager->AcqRun = true;
		PKU_DGTZ_RunManager->PlotFlag = false;
		DoInTerminal("rm -f online.csv");
	      }
	    break;
	  }


	case 'g':
	  {
	    if(PKU_DGTZ_RunManager->AcqRun) 
	      {
		printf("Please enter [s] to stop and enter [g] to get traces.\n");
		break;
	      }
	    else
	      {
		FILE * fil;
		unsigned int adc0[NTRACE_SAMPLES], adc1[NTRACE_SAMPLES], adc2[NTRACE_SAMPLES], adc3[NTRACE_SAMPLES];
		int k;
		DoInTerminal("rm -f ADC.csv");

		// read 8K samples from ADC register 
		// at this point, no guarantee that sampling is truly periodic
		mapped[AOUTBLOCK] = OB_EVREG;// switch reads to event data block of addresses

		// dummy reads for sampling update
		k = mapped[AADC0] & 0xFFFF;
		k = mapped[AADC1] & 0xFFFF;
		k = mapped[AADC2] & 0xFFFF;
		k = mapped[AADC3] & 0xFFFF;

		for(k = 0; k < NTRACE_SAMPLES; k ++ )
		  adc0[k] = mapped[AADC0] & 0xFFFF;
		for( k = 0; k < NTRACE_SAMPLES; k ++ )
		  adc1[k] = mapped[AADC1] & 0xFFFF;
		for( k = 0; k < NTRACE_SAMPLES; k ++ )
		  adc2[k] = mapped[AADC2] & 0xFFFF;
		for( k = 0; k < NTRACE_SAMPLES; k ++ )
		  adc3[k] = mapped[AADC3] & 0xFFFF;

		// open the output file
		fil = fopen("ADC.csv","w");
		fprintf(fil,"sample,Ch0,Ch1,Ch2,Ch3\n");

		//  write to file
		for(k = 0; k < NTRACE_SAMPLES; k ++ )
		  {
		    fprintf(fil,"%d,%d,%d,%d,%d\n ",k,adc0[k],adc1[k],adc2[k],adc3[k]);
		  }
 
		fclose(fil);// clean up  
	      }
	    break;
	  }

	case 'p':
	  {
	    if(PKU_DGTZ_RunManager->PlotFlag)
	      {//ploting,do stop
		PKU_DGTZ_RunManager->PlotFlag = false;
		DoInTerminal("rm -f online.csv");
	      }
	    else
	      {// not plot ,do start
		PKU_DGTZ_RunManager->PlotFlag = true;
		PKU_DGTZ_RunManager->DoPlotChannel = 0;
		PKU_DGTZ_RunManager->PlotRecent = true;
	      }
	    break;
	  }

	case '0':
	  {
	    PKU_DGTZ_RunManager->PlotRecent = true;
	    break;
	  }

	case '4':
	  {
	    if(PKU_DGTZ_RunManager->DoPlotChannel > 0)
	      {
		PKU_DGTZ_RunManager->DoPlotChannel--;
		PKU_DGTZ_RunManager->PlotRecent = true;
	      }
	    break;
	  }

	case '6':
	  {
	    if(PKU_DGTZ_RunManager->DoPlotChannel < NCHANNELS-1)
	      {
		PKU_DGTZ_RunManager->DoPlotChannel++;
		PKU_DGTZ_RunManager->PlotRecent = true;
	      }
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

void InitFPGA(volatile unsigned int *mapped, struct PixieNetFippiConfig *config, struct  DigitizerFPGAUnit *fpgapar)
{
  unsigned int  mval;
  int addr;

  unsigned int saveR2[NCHANNELS];
  unsigned int gain[NCHANNELS*2];
  unsigned int PSAM, PSEP;
  unsigned int QDCL0[NCHANNELS], QDCL1[NCHANNELS], QDCD0[NCHANNELS], QDCD1[NCHANNELS];
  unsigned int i2cdata[8];

  // Init

  // first, set CSR run control options   
  mapped[ACSRIN] = 0x0000; // all off
  mapped[AOUTBLOCK] = OB_IOREG;	  // read from IO block

  mval = config->COINCIDENCE_PATTERN;
  mapped[ACOINCPATTERN] = mval;
  if(mapped[ACOINCPATTERN] != mval) printf("Error writing value COINCIDENCE_PATTERN register\n");
     
  mval = (int)floor( (config->HV_DAC/5.0) * 65535);		// map 0..5V range to 0..64K	
  mapped[AHVDAC] = mval;
  if(mapped[AHVDAC] != mval) printf("Error writing to HV_DAC register\n");
  usleep(DACWAIT);		// wait for programming
  mapped[AHVDAC] = mval;     // repeat, sometimes doesn't take?
  if(mapped[AHVDAC] != mval) printf("Error writing to HV_DAC register\n");
  usleep(DACWAIT);

  mapped[ASERIALIO] = config->SERIAL_IO;
  if(mapped[ASERIALIO] != config->SERIAL_IO) printf("Error writing to SERIAL_IO register\n");
  usleep(DACWAIT);		// wait for programming

  mapped[AAUXCTRL] = config->AUX_CTRL;
  if(mapped[AAUXCTRL] != config->AUX_CTRL) printf("Error writing AUX_CTRL register\n");

  for(int k = 0; k < NCHANNELS; k ++ )
    {
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
      mval = (config->CHANNEL_CSRA[k] + (config->CHANNEL_CSRC[k] << 16));
      mapped[addr+0] = mval;
      if(mapped[addr+0] != mval) printf("Error writing to CHANNEL_CSR register\n");
    }

    for(int k = 0; k < NCHANNELS; k ++ )
    {
      mval = fpgapar->SL[k]-1;
      mval = mval + ((fpgapar->SL[k]+fpgapar->SG[k]-1)     <<  8);
      mval = mval + ((fpgapar->SG[k]-1)           << 16);
      mval = mval + (((2*fpgapar->SL[k]+fpgapar->SG[k])/4) << 24);
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
      mapped[addr+1] = mval;
      if(mapped[addr+1] != mval) printf("Error writing parameters to ENERGY_FILTER register\n");
    }

    for(int k = 0; k < NCHANNELS; k ++ )
      {
	mval = fpgapar->FL[k]-1;
	mval = mval + ((fpgapar->FL[k]+fpgapar->FG[k]-1) << 8);
	mval = mval + ((fpgapar->TH[k]) << 16);
	mval = mval + ((fpgapar->FR) << 26);
	saveR2[k] = mval;
	mval = mval + (1 << 31); 
	addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
	mapped[addr+2] = mval;
	if(mapped[addr+2] != mval) printf("Error writing parameters to trigger filter register\n");
      }

    for(int k = 0; k < NCHANNELS; k ++ )
      {
	if(config->ANALOG_GAIN[k] > (GAIN_HIGH+GAIN_LOW)/2 ) 
	  {
	    gain[2*k+1] = 1;      // 2'b10 = gain 5
	    gain[2*k]   = 0;   
	  }
	else  
	  {
	    gain[2*k+1] = 0;      
	    gain[2*k]   = 1;      // 2'b01 = gain 2
	  }
	PSAM = fpgapar->SL[k]+fpgapar->SG[k]-5;       
	PSEP = (PSAM+6) * (1 << fpgapar->FR);
	PSEP = 8192 - PSEP;
	mval = PSAM;
	mval = mval + (PSEP        << 13);
	mval = mval + (gain[2*k]   << 26);
	mval = mval + (gain[2*k+1] << 27);
	addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
	mapped[addr+3] = mval;
	if(mapped[addr+3] != mval) printf("Error writing parameters to gain register\n");
	// no limits for DIG_GAIN
      }

   for(int k = 0; k < NCHANNELS; k ++ )
   {
      unsigned int dac = (int)floor((1 - config->VOFFSET[k]/ V_OFFSET_MAX) * 32768);	
      mval = dac;
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
      mapped[addr+4] = mval;
      if(mapped[addr+4] != mval) printf("Error writing parameters to DAC register\n");
      usleep(DACWAIT);		// wait for programming
      mapped[addr+4] = mval;     // repeat, sometimes doesn't take?
      if(mapped[addr+4] != mval) printf("Error writing parameters to DAC register\n");
      usleep(DACWAIT);     
   }

   for(int k = 0; k < NCHANNELS; k ++ )
   {
      mval = (fpgapar->TD[k]+TWEAK_UD)/4;           // add tweak to accomodate trigger pipelining delay
      mval = mval + (fpgapar->TL[k]>0)*(1<<29);     // set bit 29 if TL is not zero
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each
      mapped[addr+5] = mval;
      if(mapped[addr+5] != mval) printf("Error writing parameters to TRACE1 register");
   
      mval = fpgapar->TL[k]/4;
      mval = mval + (fpgapar->CW       <<  16);
      mval = mval + (config->PSA_THRESHOLD[k]  <<  24);  // 
      mapped[addr+6] = mval;
      if(mapped[addr+6] != mval) printf("Error writing parameters to TRACE2 register");
   }

   for(int k = 0; k < NCHANNELS; k ++ )
     {
       mval = fpgapar->GW[k];
       mval = mval + (fpgapar->GD[k]    <<  8);
       addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each     
       mapped[addr+7] = mval;
       if(mapped[addr+7] != mval) printf("Error writing parameters to GATE register");
     }

   for(int k = 0; k < NCHANNELS; k ++ )
   {
      mval = (int)floor( config->COINC_DELAY[k] * ADC_CLK_MHZ);    
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each     
      mapped[addr+9] = mval;
      if(mapped[addr+9] != mval) printf("Error writing parameters to COINC_DELAY register");
   }

   for(int k = 0; k < NCHANNELS; k ++ )
   { 
      // 250 MHz implementation works on 2 samples at a time, so divide by 2
      QDCL0[k] = (int)floorf( config->QDC0_LENGTH[k]/2.0)+1;  
      QDCD0[k] = config->QDC0_DELAY[k] + QDCL0[k]*2;
      QDCL1[k] = (int)floorf( config->QDC1_LENGTH[k]/2.0)+1;
      QDCD1[k] = config->QDC1_DELAY[k] + QDCL1[k]*2;
      mval = QDCL0[k];
      mval = mval + (QDCD0[k] <<  7);
      mval = mval + (QDCL1[k] <<  16);
      mval = mval + (QDCD1[k] <<  23);
      mval = mval + (1<<15);     // set bit 15 for 2x correction for QDC0 (always)
      mval = mval + (1<<31);     // set bit 31 for 2x correction for QDC1 (always)
      if( config->QDC_DIV8[k])  {
         mval = mval | (1<<5);      // set bits to divide result by 8
         mval = mval | (1<<21);
      }

      // optional division by 8 of output sums not implemented, controlled by MCSRB bits
      addr = N_PL_IN_PAR+k*N_PL_IN_PAR;   // channel registers begin after NPLPAR system registers, NPLPAR each     
      mapped[addr+10] = mval;
      if(mapped[addr+10] != mval) printf("Error writing parameters to QDC register");
   }


   // restart/initialize filters 
   usleep(100);      // wait for filter FIFOs to clear, really should be longest SL+SG
   for(int k = 0; k < NCHANNELS; k ++ )
     {
       addr = 16+k*16;
       mapped[addr+2] = saveR2[k];       // restart filters with the halt bit in R2 set to zero
     }
   usleep(100);      // really should be longest SL+SG
   mapped[ADSP_CLR] = 1;
   mapped[ARTC_CLR] = 1;



   // ************************ I2C programming *********************************
   // gain and termination applied across all channels via FPGA's I2C
   // TODO
   // I2C connects to gain enables, termination relays, thermometer, PROM (with s/n etc), optional external

   // ---------------------- program gains -----------------------

   I2Cstart(mapped);

   // I2C addr byte
   i2cdata[7] = 0;
   i2cdata[6] = 1;
   i2cdata[5] = 0;
   i2cdata[4] = 0;
   i2cdata[3] = 0;   // A2
   i2cdata[2] = 1;   // A1
   i2cdata[1] = 0;   // A0
   i2cdata[0] = 0;   // R/W*
   I2Cbytesend(mapped, i2cdata);
   I2Cslaveack(mapped);

   // I2C data byte
   for(int k = 0; k <8; k++ )     // NCHANNELS*2 gains, but 8 I2C bits
     {
       i2cdata[k] = gain[k];
     }
   I2Cbytesend(mapped, i2cdata);
   I2Cslaveack(mapped);

   // I2C data byte
   I2Cbytesend(mapped, i2cdata);      // send same bits again for enable?
   I2Cslaveack(mapped);

   I2Cstop(mapped);

   // ---------------------- program termination -----------------------

   I2Cstart(mapped);

   // I2C addr byte
   i2cdata[7] = 0;
   i2cdata[6] = 1;
   i2cdata[5] = 0;
   i2cdata[4] = 0;
   i2cdata[3] = 0;   // A2
   i2cdata[2] = 0;   // A1
   i2cdata[1] = 1;   // A0
   i2cdata[0] = 0;   // R/W*
   I2Cbytesend(mapped, i2cdata);
   I2Cslaveack(mapped);

   // I2C data byte
   // settings taken from MCSRB
   i2cdata[7] = (config->MODULE_CSRB & 0x0080) >> 7 ;    // power down ADC driver D, NYI
   i2cdata[6] = (config->MODULE_CSRB & 0x0040) >> 6 ;    // power down ADC driver C, NYI
   i2cdata[5] = (config->MODULE_CSRB & 0x0020) >> 5 ;    // power down ADC driver B, NYI
   i2cdata[4] = (config->MODULE_CSRB & 0x0010) >> 4 ;    // power down ADC driver A, NYI
   i2cdata[3] = (config->MODULE_CSRB & 0x0008) >> 3 ;    //unused
   i2cdata[2] = (config->MODULE_CSRB & 0x0004) >> 2 ;    // term. CD
   i2cdata[1] = (config->MODULE_CSRB & 0x0002) >> 1 ;    // term. AB
   i2cdata[0] = (config->MODULE_CSRB & 0x0001)      ;    //unused
   I2Cbytesend(mapped, i2cdata);
   I2Cslaveack(mapped);

   // I2C data byte
   I2Cbytesend(mapped, i2cdata);      // send same bits again for enable?
   I2Cslaveack(mapped);

   I2Cstop(mapped);

  
   // ************************ end I2C *****************************************

   // ADC board temperature
   printf("ADC board temperature: %d C \n",(int)board_temperature(mapped) );

   // ***** ZYNQ temperature
   printf("Zynq temperature: %d C \n",(int)zynq_temperature() );

   // ***** check HW info *********
   if(hwinfo(mapped)==0) printf("WARNING: HW may be incompatible with this SW/FW \n");


}

