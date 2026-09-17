#include "BTConfig.H"

#include <sys/stat.h>

#ifdef STAT_MACROS_BROKEN
# define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif


#include <iostream>
using namespace std;

#include <string.h>

#include "ParsedFile.H"
#include "BTConfigFile.H"

static char BTCONFIGFILE_DEFPATH[] = "/";

BTConfigFile::BTConfigFile(const char *configfile)
: datadir_(0), logsdir_(0), pipedir_(0), slvpath_(0), audiodir_(0), artdir_(0),
  status_(BTCONFIGFILE_OK)
{
  if(configfile == 0) {
    status_ = BTCONFIGFILE_BADFILE;
    return;
  }

  ParsedFile cf(configfile);
  char *varname;

  if(cf.fail()) {
    cerr << "\"" << configfile << "\": Failed to open config file" << endl;
    status_ = BTCONFIGFILE_BADFILE;
    return;
  }

  for(int line = 1; !cf.eof(); line++) {
    cf.parseline();

    if(cf.ntokens() == 0)
      continue;

    if(cf.ntokens() != 2) {
      cerr << "\"" << configfile << "\", line " << line
           << ": Malformed configuration directive." << endl;
      status_ = BTCONFIGFILE_CONFERR;
      return;
    }

    varname = cf.token();

    if(strcmp(varname, "DATADIR") == 0) {
      if(!verifydir(&datadir_, cf.token(), configfile, line))
        datadir_ = BTCONFIGFILE_DEFPATH;
    } else if(strcmp(varname, "LOGSDIR") == 0) {
      if(!verifydir(&logsdir_, cf.token(), configfile, line))
        logsdir_ = BTCONFIGFILE_DEFPATH;
    } else if(strcmp(varname, "PIPEDIR") == 0) {
      if(!verifydir(&pipedir_, cf.token(), configfile, line))
        pipedir_ = BTCONFIGFILE_DEFPATH;
    } else if(strcmp(varname, "SLVPATH") == 0) {
      if(!verifyfile(&slvpath_, cf.token(), configfile, line))
        slvpath_ = BTCONFIGFILE_DEFPATH;
    } else if(strcmp(varname, "AUDIODIR") == 0) {
      if(!verifydir(&audiodir_, cf.token(), configfile, line))
        audiodir_ = BTCONFIGFILE_DEFPATH;
    } else if(strcmp(varname, "ARTDIR") == 0) {
      if(!verifydir(&artdir_, cf.token(), configfile, line))
        artdir_ = BTCONFIGFILE_DEFPATH;
    } else {
      cerr << "\"" << configfile << "\", line " << line
           << ": Invalid configuration variable." << endl;
      status_ = BTCONFIGFILE_CONFERR;
      return;
    }
  }
}

BTConfigFile::~BTConfigFile()
{
  char *paths[] = {datadir_, logsdir_, pipedir_, slvpath_, audiodir_, artdir_};
  for (unsigned i = 0; i < 6; ++i)
    if (paths[i] != BTCONFIGFILE_DEFPATH) delete [] paths[i];
}

BTConfigFile::BTConfigFile(const BTConfigFile& other)
: datadir_(0), logsdir_(0), pipedir_(0), slvpath_(0), audiodir_(0), artdir_(0),
  status_(BTCONFIGFILE_OK)
{
  *this = other;
}

BTConfigFile& BTConfigFile::operator=(const BTConfigFile& other)
{
  if (this == &other) return *this;
  char **dest[] = {&datadir_, &logsdir_, &pipedir_, &slvpath_, &audiodir_, &artdir_};
  const char *source[] = {other.datadir_, other.logsdir_, other.pipedir_,
                         other.slvpath_, other.audiodir_, other.artdir_};
  for (unsigned i = 0; i < 6; ++i) {
    char *copy = source[i] ? new char[strlen(source[i]) + 1] : 0;
    if (copy) strcpy(copy, source[i]);
    if (*dest[i] != BTCONFIGFILE_DEFPATH) delete [] *dest[i];
    *dest[i] = copy;
  }
  status_ = other.status_;
  return *this;
}

int BTConfigFile::verifyfile(char **bufaddr, const char *token,
                           const char *file, int line)
{
  struct stat sbuf;

  if (*bufaddr != BTCONFIGFILE_DEFPATH) delete [] *bufaddr;
  *bufaddr = 0;

  if(stat(token, &sbuf) < 0) {
    cerr << "\"" << file << "\", line " << line
         << ": File " << token << " does not exist." << endl;
    status_ = BTCONFIGFILE_CONFERR;
    return 0;
  }

  if(!S_ISREG(sbuf.st_mode)) {
    cerr << "\"" << file << "\", line " << line
         << ": " << token << " does not refer to a file." << endl;
    status_ = BTCONFIGFILE_CONFERR;
    return 0;
  }

  if((*bufaddr = new char [strlen(token) + 1]) == 0) {
    status_ = BTCONFIGFILE_MEMERR;
    return 0;
  }

  strcpy(*bufaddr, token);

  return 1;
}

int BTConfigFile::verifydir(char **bufaddr, const char *token,
                          const char *file, int line)
{
  struct stat sbuf;

  if (*bufaddr != BTCONFIGFILE_DEFPATH) delete [] *bufaddr;
  *bufaddr = 0;

  if(stat(token, &sbuf) < 0) {
    cerr << "\"" << file << "\", line " << line
         << ": Directory " << token << " does not exist." << endl;
    status_ = BTCONFIGFILE_CONFERR;
    return 0;
  }

  if(!S_ISDIR(sbuf.st_mode)) {
    cerr << "\"" << file << "\", line " << line
         << ": " << token << " does not refer to a directory." << endl;
    status_ = BTCONFIGFILE_CONFERR;
    return 0;
  }

  if((*bufaddr = new char [strlen(token) + 1]) == 0) {
    status_ = BTCONFIGFILE_MEMERR;
    return 0;
  }

  strcpy(*bufaddr, token);

  return 1;
}
