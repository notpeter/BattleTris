#include "BTConfigFile.H"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
  char directory[] = "/tmp/battletris-config-XXXXXX";
  assert(mkdtemp(directory));
  const std::string filename = std::string(directory) + "/config";
  {
    std::ofstream file(filename.c_str());
    for (const char *key : {"DATADIR", "LOGSDIR", "PIPEDIR", "AUDIODIR", "ARTDIR", "ARTDIR"})
      file << key << " \"" << directory << "\"\n";
    file << "SLVPATH \"/bin/sh\"\n";
  }
  {
    BTConfigFile original(filename.c_str()), copy(original), assigned(0);
    assert(original.status() == BTCONFIGFILE_OK);
    assigned = original;
    assigned = assigned;
    for (BTConfigFile *config : {&copy, &assigned}) {
      assert(config->status() == BTCONFIGFILE_OK);
      for (const char *path : {config->datadir(), config->logsdir(), config->pipedir(),
                               config->audiodir(), config->artdir()})
        assert(path && std::strcmp(path, directory) == 0);
      assert(std::strcmp(config->slvpath(), "/bin/sh") == 0);
      assert(config->artdir() != original.artdir());
    }
    std::ofstream file(filename.c_str());
    file << "ARTDIR \"" << filename << "\"\n"; // A file is not a directory.
    file.close();
    BTConfigFile invalid(filename.c_str());
    assert(invalid.status() == BTCONFIGFILE_CONFERR);
    assigned = invalid;
    assert(assigned.status() == invalid.status());
    assigned = original; // Replace a copied fallback path.
    invalid = original; // Replace the static fallback path.
  }
  unlink(filename.c_str());
  rmdir(directory);
}
