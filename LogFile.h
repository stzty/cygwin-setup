/*
 * Copyright (c) 2002, Robert Collins..
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#include "LogSingleton.h"

// Logging class. Default logging level is PLAIN.
class LogFile : public LogSingleton {
public:
  LogFile();
  void clearFiles(); // delete all target filenames
  void setFile (int minlevel, String const &path, bool append);
  /* Some platforms don't call destructors. So this call exists
   * which guarantees to flush any log data...
   * but doesn't call generic C++ destructors
   */
  virtual void exit (int const exit_code) __attribute__ ((noreturn));
  virtual ~LogFile();
  // get a specific verbosity stream.
  virtual ostream &operator() (enum log_level level);
  
protected:
  LogFile (LogFile const &); // no copy constructor
  LogFile &operator = (LogFile const&); // no assignment operator
  virtual void endEntry(); // the current in-progress entry is complete.
private:
  void log_save (int babble, String const &filename, bool append);
};